// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <list>
#include <sstream>
#include "EmuTime.hh"	// for uint64
#include "MSXException.hh"
#include "libxmlx/xmlx.hh"


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
		
		private:
			// block usage
			Parameter(const Parameter &);
			Parameter &operator=(const Parameter &);
		};

		Config(XML::Element *element);
		virtual ~Config();

		virtual bool isDevice();
		virtual const std::string &getType();
		virtual const std::string &getId();
		virtual const std::string &getDesc();
		virtual const std::string &getRem();

		virtual bool hasParameter(const std::string &name);
		virtual const std::string &getParameter(const std::string &name);
		virtual const bool getParameterAsBool(const std::string &name);
		virtual const int getParameterAsInt(const std::string &name);
		virtual const uint64 getParameterAsUint64(const std::string &name);

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

		virtual void dump();
	private:
		XML::Element* element;
		XML::Element* getParameterElement(const std::string &name);
		
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
			
			bool hasSS();
			bool hasPage();
			int getPS();
			int getSS();
			int getPage();

			void dump();
			
		private:
			// block usage
			Slotted(const Slotted &);
			Slotted &operator=(const Slotted &);
			
			int ps;
			int ss;
			int page;
		};

		Device(XML::Element *element);
		virtual ~Device();

		virtual bool isDevice();
		bool  isSlotted();

		std::list <Slotted*> slotted;

		virtual void dump();
		
	private:
		// block usage
		Device(const Device &foo);
		Device &operator=(const Device &foo);
};


class MSXConfig
{
	public:
		/**
		 * load a config file's content, and add it
		 * to the config data [can be called multiple
		 * times]
		 */
		virtual void loadFile(const std::string &filename);
		virtual void loadStream(const std::ostringstream &stream);

		/**
		 * save current config to file
		 */
		virtual void saveFile();
		virtual void saveFile(const std::string &filename);

		/**
		 * get a config or device or customconfig by id
		 */
		virtual Config* getConfigById(const std::string &id);
		virtual Device* getDeviceById(const std::string &id);

		/**
		 * get a device
		 */
		virtual void initDeviceIterator();
		virtual Device* getNextDevice();

		/**
		 * returns the one backend, for backwards compat
		 */
		static MSXConfig* instance();

		virtual ~MSXConfig();

	protected:
		MSXConfig();

	private:
		// block usage
		MSXConfig(const MSXConfig &foo);
		MSXConfig &operator=(const MSXConfig &foo);

		bool hasConfigWithId(const std::string &id);
		void handleDoc(XML::Document* doc);

		std::list<XML::Document*> docs;
		std::list<Config*> configs;
		std::list<Device*> devices;

		std::list<Device*>::const_iterator device_iterator;
};

#endif
