/*
** File: tms9928a.h -- software implementation of the TMS9928A VDP.
**
** By Sean Young 1999 (sean@msxnet.org).
*/


#define TMS9928A_PALETTE_SIZE           16
#define TMS9928A_COLORTABLE_SIZE        16

/*
** The different models
*/

#define TMS99x8A	(1)
#define TMS99x8		(2)

/*
** The init, reset and shutdown functions
*/
int TMS9928A_start (int model, unsigned int vram);
void TMS9928A_reset (void);
void TMS9928A_stop (void);
void tms9928A_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

/*
** The I/O functions
*/
READ_HANDLER (TMS9928A_vram_r);
WRITE_HANDLER (TMS9928A_vram_w);
READ_HANDLER (TMS9928A_register_r);
WRITE_HANDLER (TMS9928A_register_w);

/*
** Call this function to render the screen.
*/
void TMS9928A_refresh (struct osd_bitmap *, int full_refresh);

/*
** This next function must be called 50 or 60 times per second,
** to generate the necessary interrupts
*/
int TMS9928A_interrupt (void);

/*
** The parameter is a function pointer. This function is called whenever
** the state of the INT output of the TMS9918A changes.
*/
void TMS9928A_int_callback (void (*callback)(int));

/*
** Set display of illegal sprites on or off
*/
void TMS9928A_set_spriteslimit (int);

