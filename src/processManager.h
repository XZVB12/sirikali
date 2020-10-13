/*
 *
 *  Copyright (c) 2020
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
#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include "task.hpp"
#include "engines.h"

#include <QStringList>
#include <QByteArray>

#include <vector>

class processManager
{
public:
	struct mountOpts
	{
	        mountOpts( const QString& a,const QString& b,const QString& c,
		       const QString& d,const QStringList& e ) :
		mode( a ),subtype( b ),cipherFolder( c ),mountPointPath( d ),fuseOptions( e )
		{
		}
		QString mode ;
		QString subtype ;
		QString cipherFolder ;
		QString mountPointPath ;
		QStringList fuseOptions ;
	} ;

	struct opts{

		bool create ;
		const engines::engine::args& args ;
		const engines::engine::cmdArgsList& options ;
		const engines::engine& engine ;
		const QByteArray& password ;
	} ;

	static processManager& get()
	{
		static processManager s ;
		return s ;
	}

	static bool backEndTimedOut( const QString& e ) ;

	Task::process::result run( const processManager::opts& ) ;
	Task::process::result add( const processManager::opts& ) ;
	Task::process::result remove( const QStringList& unMountCommand,const QString& mountPoint ) ;
	std::vector< QStringList > commands() const ;
	std::vector< engines::engine::Wrapper > enginesList() const ;
	QString volumeProperties( const QString& mountPath ) ;
	void updateVolumeList( std::function< void() > ) ;
	std::vector< processManager::mountOpts > mountOptions() ;
	bool mountPointTaken( const QString& e ) ;
private:
	class Process
	{
	public:
		template< typename T >
		Process( const processManager::opts& opts,T x ) :
			m_args( opts.args ),
			m_engine( opts.engine ),
			m_environment( m_engine->getProcessEnvironment() ), //we get this variable now because sshfs backend can change it under us.
			m_instance( std::move( x ) )
		{
		}
		Process( Process&& ) = default ;
		Process& operator=( Process&& ) = default ;
		const QString& mountPoint() const
		{
			return m_args.mountPath ;
		}
		const QProcessEnvironment& env() const
		{
			return m_environment ;
		}
		const engines::engine& engine() const
		{
			return m_engine.get() ;
		}
		const engines::engine::args& args() const
		{
			return m_args ;
		}
		QProcess& exe() const
		{
			return *m_instance ;
		}
		QStringList arguments() const
		{
			return this->exe().arguments() ;
		}
		bool running() const
		{
			return this->exe().state() == QProcess::Running ;
		}
		bool notRunning() const
		{
			return this->exe().state() == QProcess::NotRunning ;
		}
	private:
		engines::engine::args m_args ;
		engines::engine::Wrapper m_engine ;
		QProcessEnvironment m_environment ;
		std::unique_ptr< QProcess,void(*)( QProcess * ) > m_instance ;
	} ;

	std::vector< Process > m_instances ;
	std::function< void() > m_updateVolumeList ;
} ;

#endif
