// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>
#include <sstream>
#include "EmuTime.hh"	// for uint64
#include "MSXException.hh"
#include "libxmlx/xmlx.hh"
#include "File.hh"


class ConfigException: public MSXException
{
	public:
		ConfigException(const std::string &descs)
			: MSXException(descs) {}
		ConfigException(const std::ostringstream &stream)
			: MSXException(stream.str()) {}
};

class Config
{
	public:
		// a parameter is the same for all backends:
		class Parameter
		{
		public:
			Parameter(const std::string &name, 
				  const std::string &value,
				  const std::string &clasz);
			~Parameter();

			const bool getAsBool() const;
			const int getAsInt() const ;
			const uint64 getAsUint64() const;
			
			const std::string name;
			const std::string value;
			const std::string clasz;

			static bool stringToBool(const std::string &str);
			static int stringToInt(const std::string &str);
			static uint64 stringToUint64(const std::string &str);
		};

		Config(XML::Element *element, const std::string &context);
		virtual ~Config();

		const std::string &getType() const;
		const std::string &getId() const;

		const FileContext& getContext() const;

		bool hasParameter(const std::string &name) const;
		const std::string &getParameter(const std::string &name) const;
		const bool getParameterAsBool(const std::string &name) const;
		const int getParameterAsInt(const std::string &name) const;
		const uint64 getParameterAsUint64(const std::string &name) const;

		/**
		 * This returns a freshly allocated list with freshly allocated
		 * Parameter objects. The caller has to clean this all up with
		 * getParametersWithClassClean
		 */
		virtual std::list<Parameter*>* getParametersWithClass(const std::string &clasz);
		/**
		 * cleanup function for getParametersWithClass
		 */
		static void getParametersWithClassClean(std::list<Parameter*>* list);
	
	private:
		XML::Element* getParameterElement(const std::string &name) const;
		
		XML::Element* element;
		const FileContext context;
};

class Device: virtual public Config
{
	public:
		// a slotted is the same for all backends:
		class Slotted
		{
		public:
			Slotted(int PS, int SS=-1, int Page=-1);
			~Slotted();
			
			bool hasSS() const;
			bool hasPage() const;
			int getPS() const;
			int getSS() const;
			int getPage() const;

		private:
			int ps;
			int ss;
			int page;
		};

		Device(XML::Element *element, const std::string &context);
		virtual ~Device();

		std::list <Slotted*> slotted;
};


class MSXConfig
{
	public:
		/**
		 * load a config file's content, and add it to
		 *  the config data [can be called multiple times]
		 */
		void loadFile(const FileContext &context,
		              const std::string &filename);
		void loadStream(const std::string &context,
		                const std::ostringstream &stream);

		/**
		 * save current config to file
		 */
		void saveFile();
		void saveFile(const std::string &filename);

		/**
		 * get a config or device or customconfig by id
		 */
		Config* getConfigById(const std::string &id);
		Device* getDeviceById(const std::string &id);

		/**
		 * get a device
		 */
		void initDeviceIterator();
		Device* getNextDevice();

		/**
		 * returns the one backend, for backwards compat
		 */
		static MSXConfig* instance();

		~MSXConfig();

	private:
		MSXConfig();

		bool hasConfigWithId(const std::string &id);
		void handleDoc(XML::Document* doc, const std::string &context);

		std::list<XML::Document*> docs;
		std::list<Config*> configs;
		std::list<Device*> devices;

		std::list<Device*>::const_iterator device_iterator;
};

#endif
