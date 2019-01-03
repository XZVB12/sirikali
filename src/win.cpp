/*
 *
 *  Copyright (c) 2018
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "win.h"
#include "utility.h"
#include "mountinfo.h"
#include "settings.h"

#include "win.hpp"

#include <utility>
#include <tuple>

namespace SiriKali{
namespace Windows{
static const char * _backEndTimedOut = "SiriKali::Windows::BackendTimedOut" ;

static void _deleteLater( QObject * e )
{
	e->deleteLater() ;
}

struct Process
{
	Process( const engines::engine::args& args,QProcess * p ) :
		args( args ),instance( p,_deleteLater )
	{
	}
	Process( Process&& ) = default ;
	Process& operator=( Process&& ) = default ;
	engines::engine::args args ;
	std::unique_ptr< QProcess,void(*)( QObject * ) > instance ;
} ;

struct result
{
	enum class Type{ Success,Failed,Timeout,Continue } ;

	result( Type t ) : type( t )
	{
	}
	result() : type( Type::Failed )
	{
	}
	Type type ;
	QByteArray outPut ;
} ;

class instances
{
public:
	Task::process::result add( const engines::engine::args& args,
				   const QByteArray& password,
				   const engines::engine::options& opts ) ;
	Task::process::result remove( const QString& mountPoint ) ;
	std::vector< QStringList > commands() const ;
	QString volumeProperties( const QString& mountPath ) ;
	void updateVolumeList( std::function< void() > ) ;
	std::vector< SiriKali::Windows::mountOptions > mountOptions() ;
private:
	std::vector< Process > m_instances ;
	std::function< void() > m_updateVolumeList ;
} ;

static SiriKali::Windows::instances& _instances()
{
	static SiriKali::Windows::instances s ;
	return s ;
}

void updateVolumeList( std::function< void() > function )
{
	_instances().updateVolumeList( std::move( function ) ) ;
}

bool backEndTimedOut( const QString& e )
{
	return e == SiriKali::Windows::_backEndTimedOut ;
}

}

}

int SiriKali::Windows::terminateProcess( unsigned long pid )
{
	return _terminateProcess( pid ) ;
}

QString SiriKali::Windows::engineInstalledDir( const QString& e )
{
	if( utility::equalsAtleastOne( e,"encfs","encfsctl" ) ){

		return _readRegistry( "SOFTWARE\\ENCFS","InstallDir" ) ;

	}else if( e == "sshfs" ){

		return _readRegistry( "SOFTWARE\\SSHFS-Win","InstallDir" ) ;

	}else if( e == "securefs" ){

		return _readRegistry( "SOFTWARE\\SECUREFS","InstallDir" ) ;

	}else if( e == "cryfs" ){

		return _readRegistry( "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{26116061-4F99-4C44-A178-2153FA396308}","InstallLocation" ) ;
	}else{
		return QString() ;
	}
}

QStringList SiriKali::Windows::engineInstalledDirs()
{
	auto m = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{26116061-4F99-4C44-A178-2153FA396308}" ;

	return { _readRegistry( "SOFTWARE\\ENCFS","InstallDir" ),
		 _readRegistry( "SOFTWARE\\SSHFS-Win","InstallDir" ),
		 _readRegistry( "SOFTWARE\\SECUREFS","InstallDir" ),
		 _readRegistry( m,"InstallLocation" ) } ;
}

std::vector< QStringList > SiriKali::Windows::instances::commands() const
{
	std::vector< QStringList > s ;

	for( const auto& it : m_instances ){

		auto e = it.instance->arguments() ;

		for( auto& m : e ){

			m = mountinfo::encodeMountPath( m ) ;
		}

		s.emplace_back( std::move( e ) ) ;
	}

	return s ;
}

void SiriKali::Windows::instances::updateVolumeList( std::function< void() > function )
{
	m_updateVolumeList = std::move( function ) ;
}

template< typename Function >
static SiriKali::Windows::result _read( QProcess& exe,Function function )
{
	QByteArray m ;
	QByteArray s ;

	int counter = 1 ;

	SiriKali::Windows::result r ;

	while( true ){

		s = exe.readAllStandardOutput() ;

		if( !s.isEmpty() ){

			m += s ;
		}

		if( m.contains( "cygfuse: initialization failed:" ) ){

			r.type = SiriKali::Windows::result::Type::Failed ;
			r.outPut = std::move( m ) ;

			break ;
		}else{
			r = function( m ) ;

			if( r.type == SiriKali::Windows::result::Type::Continue ){

				utility::Task::suspendForOneSecond() ;
				counter++ ;

			}else if( r.type == SiriKali::Windows::result::Type::Timeout ){

				break ;
			}else{
				r.outPut = std::move( m ) ;

				break ;
			}
		}
	}

	auto a = QString::number( counter ) ;

	auto b = "Backend took " + a + " seconds to finish\nGenerated backend out:\n" ;

	utility::debug() << b + r.outPut ;

	return r ;
}

static SiriKali::Windows::result _getProcessOutput( QProcess& exe,const QString& type )
{
	if( type == "cryfs" ){

		return _read( exe,[]( const QString& e )->SiriKali::Windows::result{

			if( e.contains( "Mounting filesystem." ) ){

				return SiriKali::Windows::result::Type::Success ;

			}else if( e.contains( "Error" ) ){

				return SiriKali::Windows::result::Type::Failed ;
			}else{
				return SiriKali::Windows::result::Type::Continue ;
			}
		} ) ;

	}else if( utility::equalsAtleastOne( type,"encfs","securefs" ) ){

		return _read( exe,[]( const QString& e )->SiriKali::Windows::result{

			if( utility::containsAtleastOne( e,"has been started","init" ) ){

				return SiriKali::Windows::result::Type::Success ;

			}else if( e.contains( "Error" ) ){

				return SiriKali::Windows::result::Type::Failed ;
			}else{
				return SiriKali::Windows::result::Type::Continue ;
			}
		} ) ;

	}else if( type == "sshfs" ){

		int counter = 0 ;
		int max = settings::instance().sshfsBackendTimeout() ;

		return _read( exe,[ & ]( const QString& e )->SiriKali::Windows::result{

			if( e.contains( "has been started" ) ){

				return SiriKali::Windows::result::Type::Success ;

			}else if( utility::containsAtleastOne( e,"ssh:","read:" ) ){

				return SiriKali::Windows::result::Type::Failed ;
			}else{
				if( counter < max ){

					counter++ ;
					return SiriKali::Windows::result::Type::Continue ;
				}else{
					return SiriKali::Windows::result::Type::Timeout ;
				}
			}
		} ) ;
	}else{
		/*
		 * Shouldnt get here
		 */
		return {} ;
	}
}

static QProcessEnvironment _update_environment( const QString& type )
{
	auto env = utility::systemEnvironment() ;

	auto m = env.value( "PATH" ) ;

	auto path = [ & ](){

		if( type == "sshfs" ){

			return SiriKali::Windows::engineInstalledDir( "sshfs" ) + "\\bin;" + m ;

		}else if( type == "encfs" ){

			return SiriKali::Windows::engineInstalledDir( "encfs" ) + "\\bin;" + m ;

		}else if( type == "cryfs" ){

			return SiriKali::Windows::engineInstalledDir( "cryfs" ) + "\\bin;" + m ;
		}else{
			return m ;
		}
	}() ;

	env.insert( "PATH",path ) ;

	return env ;
}

Task::process::result SiriKali::Windows::create( const engines::engine::args& args,
						 const QByteArray& password,
						 const engines::engine::options& opts,
						 bool autoMountsOnSuccess )
{
	if( autoMountsOnSuccess ){

		return SiriKali::Windows::mount( args,password,opts ) ;
	}else{
		auto s = utility::unwrap( Task::process::run( args.cmd,password ) ) ;

		utility::logCommandOutPut( s,args.cmd ) ;

		return s ;
	}
}

static std::pair< Task::process::result,QString > _terminate_process( QProcess& e )
{
	auto cmd = e.program() ;

	QString exe ;

	auto env = [ & ](){

		if( cmd.endsWith( "encfs.exe" ) ){

			return _update_environment( "encfs" ) ;
		}else{
			return utility::systemEnvironment() ;
		}
	}() ;

	//if( cmd.endsWith( "encfs.exe" ) ){

	//	exe = "\"" + cmd + "\" -u " + s.args.mountPath ;

	//}
	if( cmd.endsWith( "securefs.exe" ) ){

		exe = "sirikali.exe terminateProcess-" + QString::number( e.processId() ) ;
	}else{
		exe = "taskkill /F /PID " + QString::number( e.processId() ) ;
	}

	auto m = utility::unwrap( Task::process::run( exe,{},-1,"",env ) ) ;

	e.waitForFinished() ;

	return { std::move( m ),exe } ;
}

Task::process::result SiriKali::Windows::instances::add( const engines::engine::args& args,
							 const QByteArray& password,
							 const engines::engine::options& opts )
{
	auto exe = utility2::unique_qptr< QProcess >() ;

	exe->setProcessEnvironment( _update_environment( opts.type ) ) ;
	exe->setProcessChannelMode( QProcess::MergedChannels ) ;
	exe->start( args.cmd ) ;
	exe->waitForStarted() ;
	exe->write( password + "\n" ) ;
	exe->closeWriteChannel() ;

	auto m = _getProcessOutput( *exe,opts.type ) ;

	auto s = [ & ](){

		if( m.type == SiriKali::Windows::result::Type::Timeout ){

			_terminate_process( *exe ) ;

			return Task::process::result( SiriKali::Windows::_backEndTimedOut,
						      QByteArray(),
						      -1,
						      0,
						      true ) ;

		}else if( m.type == SiriKali::Windows::result::Type::Success ){

			exe->closeReadChannel( QProcess::StandardError ) ;
			exe->closeReadChannel( QProcess::StandardOutput ) ;

			m_instances.emplace_back( args,exe.release() ) ;

			m_updateVolumeList() ;

			return Task::process::result( 0 ) ;
		}else{
			exe->waitForFinished() ;

			QByteArray stdOut ;
			QByteArray stdError ;

			if( opts.type == "encfs" ){

				stdOut = m.outPut ;
			}else{
				stdError = m.outPut ;
			}

			return Task::process::result( stdOut,
						      stdError,
						      exe->exitCode(),
						      exe->exitStatus(),
						      true ) ;
		}
	}() ;

	utility::logCommandOutPut( s,args.cmd ) ;

	return s ;
}

enum class encode{ True,False } ;
static QString _make_path( QString e,encode s )
{
	if( e.startsWith( "\"" ) ){

		e.remove( 0,1 ) ;
	}

	if( e.endsWith( "\"" ) ){

		e.truncate( e.size() -1 ) ;
	}

	if( s == encode::True ){

		return mountinfo::encodeMountPath( e ) ;
	}else{
		return e ;
	}
}

Task::process::result SiriKali::Windows::instances::remove( const QString& mountPoint )
{
	for( size_t i = 0 ; i < m_instances.size() ; i++ ){

		const auto& s = m_instances[ i ] ;

		if( s.args.mountPath == mountPoint ){

			auto m = _terminate_process( *s.instance ) ;

			auto r = [ & ](){

				if( m.first.success() ) {

					m_instances.erase( m_instances.begin() + static_cast< int >( i ) ) ;

					m_updateVolumeList() ;

					return Task::process::result( 0 ) ;
				}else{
					return Task::process::result( "","Failed To Terminate A Process",1,0,true ) ;
				}
			}() ;

			utility::logCommandOutPut( r,m.second ) ;

			return r ;
		}
	}

	return Task::process::result() ;
}

QString SiriKali::Windows::instances::volumeProperties( const QString& mm )
{
	auto mountPath = "\"" + mm + "\"" ;

	for( size_t i = 0 ; i < m_instances.size() ; i++ ){

		const auto& e = m_instances[ i ] ;

		if( mountPath == e.args.mountPath ){

			auto m = QObject::tr( "Mount Options:\n\n" ) ;

			for( const auto& it : e.instance->arguments() ){

				if( it != "-o" ){

					m += it + "\n" ;
				}
			}

			return m ;
		}
	}

	return QString() ;
}

std::vector< SiriKali::Windows::mountOptions > SiriKali::Windows::instances::mountOptions()
{
	std::vector< SiriKali::Windows::mountOptions > mOpts ;

	for( const auto& it : m_instances ){

		const auto& m = it.args ;

		auto a = _make_path( m.cipherPath,encode::True ) ;
		auto b = _make_path( m.mountPath,encode::True ) ;

		mOpts.emplace_back( m.mode,m.subtype,a,b,m.fuseOptions ) ;
	}

	return mOpts ;
}

QString SiriKali::Windows::volumeProperties( const QString& mountPath )
{
	return _instances().volumeProperties( mountPath ) ;
}

std::vector< SiriKali::Windows::mountOptions > SiriKali::Windows::getMountOptions()
{
	return _instances().mountOptions() ;
}

Task::process::result SiriKali::Windows::unmount( const QString& m )
{
	return _instances().remove( m ) ;
}

Task::process::result SiriKali::Windows::mount( const engines::engine::args& args,
						const QByteArray& password,
						const engines::engine::options& opts )
{
	return _instances().add( args,password,opts ) ;
}