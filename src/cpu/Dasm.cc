// $Id$

#include <cstdio>
#include <cstring>
#include "DasmTables.hh"

namespace openmsx {

static char Sign(unsigned char a)
{
	return (a & 128) ? '-' : '+';
}

static int Abs(unsigned char a)
{
	if (a & 128) return 256 - a;
	else return a;
}

int Dasm(unsigned char *buffer, char *dest, unsigned int PC)
{
	const char *S;
	//unsigned char buf[10];
	char buf[10];
	int i=0;
	char Offset=0;
	const char* r = "INTERNAL PROGRAM ERROR";
	dest[0]='\0';
	switch (buffer[i]) {
		case 0xCB:
			i++;
			S=mnemonic_cb[buffer[i++]];
			break;
		case 0xED:
			i++;
			S=mnemonic_ed[buffer[i++]];
			break;
		case 0xDD:
			i++;
			r="ix";
			switch (buffer[i])
			{
				case 0xcb:
					i++;
					Offset=buffer[i++];
					S=mnemonic_xx_cb[buffer[i++]];
					break;
				default:
					S=mnemonic_xx[buffer[i++]];
					break;
			}
			break;
		case 0xFD:
			i++;
			r="iy";
			switch (buffer[i]) {
				case 0xcb:
					i++;
					Offset=buffer[i++];
					S=mnemonic_xx_cb[buffer[i++]];
					break;
				default:
					S=mnemonic_xx[buffer[i++]];
					break;
			}
			break;
		default:
			S=mnemonic_main[buffer[i++]];
			break;
	}
	for (int j = 0; S[j]; j++) {
		switch (S[j]) {
			case 'B':
				sprintf (buf,"#%02X",buffer[i++]);
				strcat (dest,buf);
				break;
			case 'R':
				sprintf (buf,"#%04X",(PC+2+(signed char)buffer[i])&0xFFFF);
				i++;
				strcat (dest,buf);
				break;
			case 'W':
				sprintf (buf,"#%04X",buffer[i]+buffer[i+1]*256);
				i+=2;
				strcat (dest,buf);
				break;
			case 'X':
				sprintf (buf,"(%s%c#%02X)",r,Sign(buffer[i]),Abs(buffer[i]));
				i++;
				strcat (dest,buf);
				break;
			case 'Y':
				sprintf (buf,"(%s%c#%02X)",r,Sign(Offset),Abs(Offset));
				strcat (dest,buf);
				break;
			case 'I':
				strcat (dest,r);
				break;
			case '!':
				sprintf (dest,"db     #ED,#%02X",buffer[1]);
				return 2;
			case '@':
				sprintf (dest,"db     #%02X",buffer[0]);
				return 1;
			case '#':
				sprintf (dest,"db     #%02X,#CB,#%02X",buffer[0],buffer[2]);
				return 2;
			case ' ': {
				int k = strlen(dest);
				if (k<6) k=7-k;
				else k=1;
				memset (buf,' ',k);
				buf[k]='\0';
				strcat (dest,buf);
				break;
			}
			default:
				buf[0]=S[j];
				buf[1]='\0';
				strcat (dest,buf);
				break;
		}
	}
	int k=strlen(dest);
	if (k<17) k=18-k;
	else k=1;
	memset (buf,' ',k);
	buf[k]='\0';
	strcat (dest,buf);
	return i;
}

} // namespace openmsx

#ifdef TEST
#include <cstdlib>
int main(int argc, char *argv[])
{
	int i;
	unsigned char buffer[16];
	char dest[32];
	memset (buffer,0,sizeof(buffer));
	for (i=1;i<17 && i<argc;++i) buffer[i-1]=strtoul(argv[i],NULL,16);
	for (i=0;i<16;++i) printf ("%02X ",buffer[i]);
	printf ("\n");
	i=openmsx::Dasm (buffer,dest,0);
	printf ("%s  - %d bytes\n",dest,i);
	return 0;
}
#endif
