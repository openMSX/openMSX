// $Id$

#ifndef __CARTRIDGESLOTMANAGER_HH__
#define __CARTRIDGESLOTMANAGER_HH__


class CartridgeSlotManager
{
	public:
		static CartridgeSlotManager* instance();

		void reserveSlot(int slot);
		void readConfig();
		void getSlot(int slot, int &ps, int &ss);
		void getSlot(int &ps, int &ss);

	private:
		CartridgeSlotManager();

		int slots[16];
};

#endif

