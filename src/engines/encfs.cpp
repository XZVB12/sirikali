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

#include "encfs.h"

#include "encfscreateoptions.h"
#include "options.h"

static engines::engine::BaseOptions _setOptions()
{
	engines::engine::BaseOptions s ;

	s.backendTimeout              = 0 ;
	s.takesTooLongToUnlock        = false ;
	s.supportsMountPathsOnWindows = true ;
	s.autorefreshOnMountUnMount   = true ;
	s.backendRequireMountPath     = true ;
	s.backendRunsInBackGround     = true ;
	s.requiresPolkit        = false ;
	s.customBackend         = false ;
	s.requiresAPassword     = true ;
	s.hasConfigFile         = true ;
	s.autoMountsOnCreate    = true ;
	s.hasGUICreateOptions   = true ;
	s.setsCipherPath        = false ;
	s.acceptsSubType        = true ;
	s.acceptsVolName        = true ;
	s.releaseURL            = "https://api.github.com/repos/vgough/encfs/releases" ;
	s.passwordFormat        = "%{password}\n%{password}" ;
	s.reverseString         = "--reverse" ;
	s.idleString            = "--idle" ;
	s.executableName        = "encfs" ;
	s.incorrectPasswordText = "Error decoding volume key, password incorrect" ;
	s.configFileArgument    = "--config" ;
	s.windowsInstallPathRegistryKey   = "SOFTWARE\\ENCFS" ;
	s.windowsInstallPathRegistryValue = "InstallDir" ;
	s.windowsUnMountCommand           = QStringList{ "taskkill","/F","/PID","%{PID}" } ;
	s.volumePropertiesCommands        = QStringList{ "encfsctl %{cipherFolder}" } ;
	s.configFileNames       = QStringList{ ".encfs6.xml","encfs6.xml",".encfs5",".encfs4" } ;
	s.fuseNames             = QStringList{ "fuse.encfs" } ;
	s.names                 = QStringList{ "encfs","encfsctl" } ;
	s.failedToMountList     = QStringList{ "Error" } ;
	s.successfulMountedList = QStringList{ "has been started" } ;
	s.notFoundCode          = engines::engine::status::encfsNotFound ;
	s.versionInfo           = { { "--version",false,2,0 } } ;

	return s ;
}

encfs::encfs() :
	engines::engine( _setOptions() ),
	m_environment( engines::engine::getProcessEnvironment() ),
	m_versionGreatorOrEqual_1_9_5( true,*this,1,9,5 )
{	
}

engines::engine::args encfs::command( const QByteArray& password,
				      const engines::engine::cmdArgsList& args,
				      bool create ) const
{
	Q_UNUSED( password )

	engines::engine::commandOptions m( *this,args ) ;

	auto exeOptions = m.exeOptions() ;

	if( create ){

		if( !args.createOptions.isEmpty() ){

			exeOptions.add( utility::split( args.createOptions,' ' ) ) ;
		}

		exeOptions.add( "--stdinpass","--standard" ) ;
	}else{
		exeOptions.add( "--stdinpass" ) ;
	}

	if( args.boolOptions.unlockInReverseMode ){

		exeOptions.add( this->reverseString() ) ;
	}

	if( utility::platformIsWindows() ){

		exeOptions.add( "-f" ) ;

		if( !utility::isDriveLetter( args.mountPoint ) ){

			/*
			 * A user is trying to use a folder as a mount path and cryfs
			 * requires the mount path to not exist and we are deleting
			 * it because SiriKali created it previously.
			 */
			utility::removeFolder( args.mountPoint,5 ) ;
		}
	}

	m_environment.remove( "ENCFS6_CONFIG" ) ;

	if( !args.configFilePath.isEmpty() ){

		if( m_versionGreatorOrEqual_1_9_5 ){

			exeOptions.add( this->configFileArgument() + "=" + args.configFilePath ) ;
		}else{
			auto& a = args.configFilePath ;

			utility::debug() << "Encfs: Setting Env Variable Of: ENCFS6_CONFIG=" + a ;
			m_environment.insert( "ENCFS6_CONFIG",a ) ;
		}
	}

	exeOptions.add( args.cipherFolder,args.mountPoint,m.fuseOpts() ) ;

	return { args,m,this->executableFullPath(),exeOptions.get() } ;
}

engines::engine::status encfs::errorCode( const QString& e,int s ) const
{
	Q_UNUSED( s )

	if( e.contains( this->incorrectPasswordText() ) ){

		return engines::engine::status::encfsBadPassword ;

	}else if( e.contains( "cygfuse: initialization failed: winfsp" ) ){

		return engines::engine::status::failedToLoadWinfsp ;
	}else{
		return engines::engine::status::backendFail ;
	}
}

const QProcessEnvironment& encfs::getProcessEnvironment() const
{
	return m_environment ;
}

void encfs::GUICreateOptions( const engines::engine::createGUIOptions& s ) const
{
	encfscreateoptions::instance( s ) ;
}

void encfs::GUIMountOptions( const engines::engine::mountGUIOptions& s ) const
{
	auto& e = options::instance( *this,s ) ;

	auto& ee = e.GUIOptions() ;

	ee.enableKeyFile = false ;

	ee.checkBoxChecked = s.mOpts.opts.unlockInReverseMode ;

	ee.updateOptions = []( const ::options::Options& s ){

		engines::engine::booleanOptions e ;

		e.unlockInReverseMode = s.checkBoxChecked ;

		return e ;
	} ;

	e.ShowUI() ;
}
