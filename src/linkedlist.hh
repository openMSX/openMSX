// $Id$

#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#include "MSXDevice.hh"
class ElementNode
{
	protected: 
		MSXDevice *device;
		ElementNode *next;
		 
	ElementNode()
	{
		device=NULL;
		next=NULL;
	}
	ElementNode(MSXDevice *dev)
	{
		device=dev;
		next=NULL;
	}
	friend class MSXDevList;
};

class MSXDevList
{
	private:
		ElementNode *Start;
		ElementNode *End;
		ElementNode *Current;
		ElementNode *tmp;
	public:
		MSXDevice *device;
		MSXDevList()
		{
			Start=End=Current=0;
			device=0;
		};
		
		~MSXDevList()
		{
			while (Start != 0 ){ 
				tmp=Start;
				Start=Start->next;
				delete tmp;
			}
		};
		
		ElementNode* toNext()
		{
			if (Current != 0 ){
				Current=Current->next;
			}
			if (Current != 0 ){
				device=Current->device;
			} else {
				device=0;
			}
			return Current;
		}	
		void fromStart()
		{
			Current=Start;
			device=Current->device;
		}
		void append(MSXDevice *dev)
		{
			tmp= new ElementNode(dev);
			if (Start==NULL){
				Start=End=Current=tmp;
			} else {
				// checken of device all bekend is
				Current=Start;
				while (Current != NULL){
					device=Current->device;
					if (device == dev){
						delete tmp;
						return;
					}
					Current=Current->next;
				}
				// onbekend dus toevoegen
				End->next=tmp;
				End=tmp;
				Current=tmp;
				
			};
			device=Current->device;
		}

};
#endif 
