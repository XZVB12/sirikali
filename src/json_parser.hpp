/*
 *
 *  Copyright ( c ) 2019
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  ( at your option ) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JSON_H
#define JSON_H

#include "json.h"

#include <QString>
#include <QStringList>
#include <QFile>

class SirikaliJson
{
public:
	enum class type{ PATH,CONTENTS } ;

	SirikaliJson( std::function< void( const QString& ) > log ) :
		m_log( std::move( log ) )
	{
	}
	SirikaliJson( const QByteArray& e,type s,std::function< void( const QString& ) > log ) :
		m_log( std::move( log ) )
	{
		this->getData( e,s ) ;
	}
	SirikaliJson( const QString& e,type s,std::function< void( const QString& ) > log ) :
		m_log( std::move( log ) )
	{
		this->getData( e.toLatin1(),s ) ;
	}
	template< typename T >
	T get( const char * key,const T& t = T() ) const
	{
		try{
			auto a = m_json.find( key ) ;

			if( a != m_json.end() ){

				return a->get< T >() ;
			}else{
				if( m_filePath.isEmpty() ){

					m_log( QString( "Warning, Key \"%1\" Not Found" ).arg( key )  ) ;
				}else{
					m_log( QString( "Warning, Key \"%1\" Not Found In Config File at: %2" ).arg( key,m_filePath )  ) ;
				}

				return t ;
			}

		}catch( ... ) {

			if( m_filePath.isEmpty() ){

				m_log( QString( "Warning, Exception thrown when searching For Key \"%1\"" ).arg( key ) ) ;
			}else{
				m_log( QString( "Warning, Exception thrown when searching For Key \"%1\" in Config File at: %2" ).arg( key,m_filePath )  ) ;
			}

			return t ;
		}
	}
	QStringList getStringList( const char * key,const QStringList& l ) const
	{
		std::vector< std::string > m ;

		for( const auto& it : l ){

			m.emplace_back( it.toStdString() ) ;
		}

		const auto e = this->get< std::vector< std::string > >( key,m ) ;

		QStringList s ;

		for( const auto& it : e ){

			s.append( it.c_str() ) ;
		}

		return s ;
	}
	QStringList getStringList( const char * key ) const
	{
		QStringList s ;

		auto e = this->get< std::vector< std::string > >( key,m_defaultStringList ) ;

		for( const auto& it : e ){

			s.append( it.c_str() ) ;
		}

		return s ;
	}
	QString getString( const char * key,const QString& defaultValue ) const
	{
		return this->get< std::string >( key,defaultValue.toStdString() ).c_str() ;
	}
	QString getString( const char * key ) const
	{
		return this->get< std::string >( key,m_defaultString ).c_str() ;
	}
	QByteArray getByteArray( const char * key,const QByteArray& defaultValue ) const
	{
		return this->get< std::string >( key,defaultValue.toStdString() ).c_str() ;
	}
	QByteArray getByteArray( const char * key ) const
	{
		return this->get< std::string >( key,m_defaultString ).c_str() ;
	}
	bool getBool( const char * key,bool s = false ) const
	{
		return this->get< bool >( key,s ) ;
	}
	int getInterger( const char * key,int s = 0 ) const
	{
		return this->get< int >( key,s ) ;
	}
	double getDouble( const char * key,double s = 0.0 ) const
	{
		return this->get< double >( key,s ) ;
	}
	template< typename T >
	void insert( const char * key,const T& value )
	{
		m_key = key ;
		*this = value ;
	}
	SirikaliJson& operator[]( const char * key )
	{
		m_key = key ;
		return *this ;
	}
	void operator=( const QString& value )
	{
		m_json[ m_key ] = value.toStdString() ;
	}
	void operator=( const QByteArray& value )
	{
		m_json[ m_key ] = value.constData() ;
	}
	void operator=( const QStringList& value )
	{
		std::vector< std::string > m ;

		for( const auto& it : value ){

			m.emplace_back( it.toStdString() ) ;
		}

		m_json[ m_key ] = m ;
	}
	template< typename T >
	void operator=( const T& value )
	{
		m_json[ m_key ] = value ;
	}
	bool toFile( const QString& path,int indent = 8 ) const
	{
		QFile file( path ) ;

		if( file.open( QIODevice::WriteOnly ) ){

			file.write( m_json.dump( indent ).data() ) ;

			return true ;
		}else{
			return false ;
		}
	}
	QByteArray structure( int indent = 8 ) const
	{
		return m_json.dump( indent ).c_str() ;
	}
	QStringList getTags( const char * tag )
	{
		QStringList s ;

		for( const auto& it : m_json ){

			auto e = it.find( tag ) ;

			if( e != it.end() ){

				s.append( QString::fromStdString( e.value() ) ) ;
			}
		}

		return s ;
	}
private:
	std::vector< std::string > m_defaultStringList ;
	std::string m_defaultString ;
	QString m_filePath ;

	void getData( const QByteArray& e,type s )
	{
		if( s == type::PATH ){

			m_filePath = e ;

			QFile file( e ) ;

			if( file.open( QIODevice::ReadOnly ) ){

				m_json = nlohmann::json::parse( file.readAll().constData() ) ;
			}
		}else{
			m_json = nlohmann::json::parse( e.constData() ) ;
		}
	}
	std::function< void( const QString& ) > m_log = []( const QString& e ){ Q_UNUSED( e ) } ;
	const char * m_key ;
	nlohmann::json m_json ;
};

#endif //JSON_H
