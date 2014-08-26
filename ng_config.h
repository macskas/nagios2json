#ifndef NG_CONFIG_H
#define NG_CONFIG_H
	#include <string>
	#include <map>
	
	typedef std::map<std::string, std::string>			config_str_t;
	typedef std::map<std::string, int>					config_int_t;
	
	class config;
	
	class config
	{
		private:
			static config *pinstance;
		public:
			static config* getInstance();
			~config();
			
		private:
			config();
			
			config_str_t	config_str;
			config_int_t	config_int;
			
		public:
			bool set(std::string, std::string);
			bool get(std::string, std::string*);
			std::string get(std::string);
			std::string get(std::string, std::string);
			
			bool setInt(std::string, int);
			bool getInt(std::string, int*);
			int getInt(std::string, int);
	};
#endif