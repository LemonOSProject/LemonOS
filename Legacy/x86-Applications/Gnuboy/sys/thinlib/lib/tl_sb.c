/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_sb.c
**
** DOS Sound Blaster routines
**
** Note: the information in this file has been gathered from many
**  Internet documents, and from source code written by Ethan Brodsky.
**
** $Id: $
*/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dos.h>
#include <dpmi.h>

#include "tl_types.h"
#include "tl_djgpp.h"
#include "tl_int.h"
#include "tl_sb.h"
#include "tl_log.h"


/* General defines */
#define  LOW_BYTE(x)          (uint8) ((x) & 0xFF)
#define  HIGH_BYTE(x)         (uint8) ((x) >> 8)

#define  INVALID              0xFFFFFFFF
#define  DEFAULT_TIMEOUT      20000

#define  DETECT_POLL_REPS     1000

#define  DSP_VERSION_SB_15    0x0200
#define  DSP_VERSION_SB_20    0x0201
#define  DSP_VERSION_SB_PRO   0x0300
#define  DSP_VERSION_SB16     0x0400

/* DSP register offsets */
#define  DSP_RESET            0x06
#define  DSP_READ             0x0A
#define  DSP_READ_READY       0x0E
#define  DSP_WRITE            0x0C
#define  DSP_WRITE_BUSY       0x0C
#define  DSP_DMA_ACK_8BIT     0x0E
#define  DSP_DMA_ACK_16BIT    0x0F
#define  DSP_RESET_SUCCESS    0xAA

/* SB 1.0 commands */
#define  DSP_DMA_TIME_CONST   0x40
#define  DSP_DMA_DAC_8BIT     0x14
#define  DSP_DMA_PAUSE_8BIT   0xD0
#define  DSP_DMA_CONT_8BIT    0xD4
#define  DSP_SPEAKER_ON       0xD1
#define  DSP_SPEAKER_OFF      0xD3
#define  DSP_GET_VERSION      0xE1

/* SB 1.5 - Pro commands */
#define  DSP_DMA_BLOCK_SIZE   0x48
#define  DSP_DMA_DAC_AI_8BIT  0x1C  /* low-speed autoinit */
#define  DSP_DMA_DAC_HS_8BIT  0x90  /* high-speed autoinit */

/* SB16 commands */
#define  DSP_DMA_DAC_RATE     0x41
#define  DSP_DMA_START_16BIT  0xB0
#define  DSP_DMA_START_8BIT   0xC0
#define  DSP_DMA_DAC_MODE     0x06
#define  DSP_DMA_PAUSE_16BIT  0xD5
#define  DSP_DMA_CONT_16BIT   0xD6
#define  DSP_DMA_STOP_8BIT    0xDA

/* DMA flags */
#define  DSP_DMA_UNSIGNED     0x00
#define  DSP_DMA_SIGNED       0x10
#define  DSP_DMA_MONO         0x00
#define  DSP_DMA_STEREO       0x20

/* DMA address/port/command defines */
#define  DMA_MASKPORT_16BIT   0xD4
#define  DMA_MODEPORT_16BIT   0xD6
#define  DMA_CLRPTRPORT_16BIT 0xD8
#define  DMA_ADDRBASE_16BIT   0xC0
#define  DMA_COUNTBASE_16BIT  0XC2
#define  DMA_MASKPORT_8BIT    0x0A
#define  DMA_MODEPORT_8BIT    0x0B
#define  DMA_CLRPTRPORT_8BIT  0x0C
#define  DMA_ADDRBASE_8BIT    0x00
#define  DMA_COUNTBASE_8BIT   0x01
#define  DMA_STOPMASK_BASE    0x04
#define  DMA_STARTMASK_BASE   0x00
#define  DMA_AUTOINIT_MODE    0x58
#define  DMA_ONESHOT_MODE     0x48

/* centerline */
#define  SILENCE_SIGNED       0x00
#define  SILENCE_UNSIGNED     0x80

/* get the irq vector number from an irq channel */
#define  SB_IRQVEC(chan)      ((chan < 8) ? (0x08 + (chan)) : (0x70 + ((chan) - 8)))


/* DOS low-memory buffer info */
static struct
{
   _go32_dpmi_seginfo buffer;
   uint32 bufaddr; /* linear address */
   uint32 offset;
   uint32 page;
} dos;


/* DMA information */
static struct
{
   volatile int count;
   uint16 addrport;
   uint16 ackport;
   bool autoinit;
} dma;

/* 8 and 16 bit DMA ports */
static const uint8 dma8_ports[4] =  { 0x87, 0x83, 0x81, 0x82 };
static const uint8 dma16_ports[4] = { 0xFF, 0x8B, 0x89, 0x8A };

/* Sound Blaster context */
static struct
{
   bool initialized;

   uint16 baseio;
   uint16 dsp_version;

   uint16 sample_rate;
   uint8 format;

   uint8 irq, dma, dma16;
   
   uint8 *buffer;
   uint32 buf_size;
   uint32 buf_chunk;
   
   sbmix_t callback;
   void *user_data;
} sb;


/*
** Basic DSP routines
*/
static void dsp_write(uint8 value)
{
   int timeout = DEFAULT_TIMEOUT;

   /* wait until DSP is ready... */
   while (timeout-- && (inportb(sb.baseio + DSP_WRITE_BUSY) & 0x80))
      ; /* loop */

   outportb(sb.baseio + DSP_WRITE, value);
}

static uint8 dsp_read(void)
{
   int timeout = DEFAULT_TIMEOUT;

   while (timeout-- && (0 == (inportb(sb.baseio + DSP_READ_READY) & 0x80)))
      ; /* loop */

   return inportb(sb.baseio + DSP_READ);
}

/* returns zero if DSP found and successfully reset, nonzero otherwise */
static int dsp_reset(void)
{
   outportb(sb.baseio + DSP_RESET, 1); /* reset command */
   delay(5);                           /* 5 usec delay */
   outportb(sb.baseio + DSP_RESET, 0); /* clear */
   delay(5);                           /* 5 usec delay */

   if (DSP_RESET_SUCCESS == dsp_read())
      return 0;

   /* BLEH, we failed */
   return -1;
}

/* return DSP version in 8:8 major:minor format */
static uint16 dsp_getversion(void)
{
   uint8 major, minor;

   dsp_write(DSP_GET_VERSION);
   
   major = dsp_read();
   minor = dsp_read();

   return ((uint16) (major << 8) | minor);
}

/*
** BLASTER environment variable parsing
*/
static int get_env_item(char *env, void *ptr, char find, int base, int width)
{
   char *item;
   int value;

   item = strrchr(env, find);
   if (NULL == item)
      return -1;

   item++;
   value = strtol(item, NULL, base);

   switch (width)
   {
   case 32:
      *(uint32 *) ptr = value;
      break;

   case 16:
      *(uint16 *) ptr = value;
      break;

   case 8:
      *(uint8 *) ptr = value;
      break;

   default:
      break;
   }

   return 0;
}

/* parse the BLASTER environment variable */
static int parse_blaster_env(void)
{
   char blaster[255 + 1], *penv;

   penv = getenv("BLASTER");

   /* bail out if we can't find it... */
   if (NULL == penv)
      return -1;

   /* copy it, normalize case */
   strncpy(blaster, penv, 255);
   strupr(blaster);

   if (get_env_item(blaster, &sb.baseio, 'A', 16, 16))
      return -1;
   if (get_env_item(blaster, &sb.irq, 'I', 10, 8))
      return -1;
   if (get_env_item(blaster, &sb.dma, 'D', 10, 8))
      return -1;
   if (get_env_item(blaster, &sb.dma16, 'H', 10, 8))
      sb.dma16 = (uint8) INVALID;

   return 0;
}

/*
** Brute force autodetection code
*/

/* detect the base IO by attempting to 
** reset the DSP at known addresses 
*/
static uint16 detect_baseio(void)
{
   int i;
   static const uint16 port_val[] =
   {
      0x210, 0x220, 0x230, 0x240,
      0x250, 0x260, 0x280, (uint16) INVALID
   };

   for (i = 0; (uint16) INVALID != port_val[i]; i++)
   {
      sb.baseio = port_val[i];
      if (0 == dsp_reset())
         break;
   }

   /* will return INVALID if not found */
   return port_val[i];
}

/* stop all DSP activity */
static void dsp_stop(void)
{
   /* pause 8/16 bit DMA mode digitized sound IO */
   dsp_reset();
   dsp_write(DSP_DMA_PAUSE_8BIT);
   dsp_write(DSP_DMA_PAUSE_16BIT);
}

/* return number of set bits in byte x */
static int bitcount(uint8 x)
{
   int i, set_count = 0;

   for (i = 0; i < 8; i++)
      if (x & (1 << i))
         set_count++;

   return set_count;
}

/* returns position of lowest bit set in byte x (INVALID if none) */
static int bitpos(uint8 x)
{
   int i;

   for (i = 0; i < 8; i++)
      if (x & (1 << i))
         return i;

   return INVALID;
}

static uint8 detect_dma(bool high_dma)
{
   uint8 dma_maskout, dma_mask;
   int i;

   /* stop DSP activity */
   dsp_stop();

   dma_maskout = ~0x10; /* initially mask only DMA4 */

   /* poll to find out which dma channels are in use */
   for (i = 0; i < DETECT_POLL_REPS; i++)
      dma_maskout &= ~(inportb(0xD0) & 0xF0) | (inportb(0x08) >> 4);

   /* TODO: this causes a pretty nasty sound */
   /* program card, see whch channel becomes active */
   if (false == high_dma)
   {
      /* 8 bit */
      dsp_write(DSP_DMA_DAC_8BIT);
   }
   else
   {
      dsp_write(DSP_DMA_START_16BIT); /* 16-bit, D/A, S/C, FIFO off */
      dsp_write(DSP_DMA_SIGNED | DSP_DMA_MONO); /* 16-bit mono signed PCM */
   }

   dsp_write(0xF0);  /* send some default length */
   dsp_write(0xFF);

   /* poll to find out which DMA channels are in use with sound */
   dma_mask = 0; /* dma channels active during audio, minus masked out */
   for (i = 0; i < DETECT_POLL_REPS; i++)
      dma_mask |= (((inportb(0xD0) & 0xF0) | (inportb(0x08) >> 4)) & dma_maskout);

   /* stop all DSP activity */
   dsp_stop();

   if (1 == bitcount(dma_mask))
      return (uint8) bitpos(dma_mask);
   else
      return (uint8) INVALID;
}

static void dsp_transfer(uint8 dma)
{
   outportb(DMA_MASKPORT_8BIT, DMA_STOPMASK_BASE | dma);

   /* write DMA mode: single-cycle read transfer */
   outportb(DMA_MODEPORT_8BIT, DMA_ONESHOT_MODE | dma);
   outportb(DMA_CLRPTRPORT_8BIT, 0x00);

   /* one transfer */
   outportb(DMA_COUNTBASE_8BIT + (2 * dma), 0x00); /* low */
   outportb(DMA_COUNTBASE_8BIT + (2 * dma), 0x00); /* high */

   /* address */
   outportb(DMA_ADDRBASE_8BIT + (2 * dma), 0x00);
   outportb(DMA_ADDRBASE_8BIT + (2 * dma), 0x00);
   outportb(dma8_ports[dma], 0x00);

   /* unmask DMA channel */
   outportb(DMA_MASKPORT_8BIT, DMA_STARTMASK_BASE | dma);

   /* 8-bit single cycle DMA mode */
   dsp_write(DSP_DMA_DAC_8BIT);
   dsp_write(0x00);
   dsp_write(0x00);
}

/*
** IRQ autodetection
*/
#define  NUM_IRQ_CHANNELS  5

static const uint8 irq_channels[NUM_IRQ_CHANNELS] = { 2, 3, 5, 7, 10 };
static volatile bool irq_hit[NUM_IRQ_CHANNELS];

#define  MAKE_IRQ_HANDLER(num) \
static int chan##num##_handler(void) { irq_hit[num] = true; return 0; } \
THIN_LOCKED_STATIC_FUNC(chan##num##_handler)

MAKE_IRQ_HANDLER(0)
MAKE_IRQ_HANDLER(1)
MAKE_IRQ_HANDLER(2)
MAKE_IRQ_HANDLER(3)
MAKE_IRQ_HANDLER(4)

static void ack_interrupt(uint8 irq)
{
   /* acknowledge the interrupts! */
   inportb(sb.baseio + 0x0E);
   if (irq > 7)
      outportb(0xA0, 0x20);
   outportb(0x20, 0x20);
}

static uint8 detect_irq(void)
{
   bool irq_mask[NUM_IRQ_CHANNELS];
   uint8 irq = (uint8) INVALID;
   int i;

   THIN_LOCK_FUNC(chan0_handler);
   THIN_LOCK_FUNC(chan1_handler);
   THIN_LOCK_FUNC(chan2_handler);
   THIN_LOCK_FUNC(chan3_handler);
   THIN_LOCK_FUNC(chan4_handler);
   THIN_LOCK_VAR(irq_hit);

   THIN_DISABLE_INTS();

   /* install temp handlers */
   thin_int_install(SB_IRQVEC(irq_channels[0]), chan0_handler);
   thin_int_install(SB_IRQVEC(irq_channels[1]), chan1_handler);
   thin_int_install(SB_IRQVEC(irq_channels[2]), chan2_handler);
   thin_int_install(SB_IRQVEC(irq_channels[3]), chan3_handler);
   thin_int_install(SB_IRQVEC(irq_channels[4]), chan4_handler);

   /* enable IRQs */
   for (i = 0; i < NUM_IRQ_CHANNELS; i++)
   {
      thin_irq_enable(irq_channels[i]);
      irq_hit[i] = false;
   }

   THIN_ENABLE_INTS();

   /* wait to see which interrupts are triggered without sound */
   delay(100);

   /* mask out any interrupts triggered without sound */
   for (i = 0; i < NUM_IRQ_CHANNELS; i++)
   {
      irq_mask[i] = irq_hit[i];
      irq_hit[i] = false;
   }

   /* try to trigger an interrupt using DSP command F2 */
   dsp_write(0xF2);

   delay(100);

   /* detect triggered interrupts */
   for (i = 0; i < NUM_IRQ_CHANNELS; i++)
   {
      if (true == irq_hit[i] && false == irq_mask[i])
      {
         irq = irq_channels[i];
         ack_interrupt(irq);
         break;
      }
   }

   /* if F2 fails to trigger an int, run a short transfer */
   if ((uint8) INVALID == irq)
   {
      dsp_reset();
      dsp_transfer(sb.dma);

      delay(100);

      /* detect triggered interrupts */
      for (i = 0; i < NUM_IRQ_CHANNELS; i++)
      {
         if (true == irq_hit[i] && false == irq_mask[i])
         {
            irq = irq_channels[i];
            ack_interrupt(irq);
            break;
         }
      }
   }

   /* reset DSP just in case */
   dsp_reset();

   THIN_DISABLE_INTS();

   /* restore IRQs to previous state, uninstall handlers */
   for (i = 0; i < NUM_IRQ_CHANNELS; i++)
   {
      thin_irq_restore(irq_channels[i]);
      thin_int_remove(SB_IRQVEC(irq_channels[i]));
   }

   THIN_ENABLE_INTS();

   return irq;
}

/* try and detect an SB without environment variables */
static int sb_detect(void)
{
   sb.baseio = detect_baseio();
   if ((uint16) INVALID == sb.baseio)
      return -1;

   sb.irq = detect_irq();
   if ((uint8) INVALID == sb.irq)
      return -1;

   sb.dma = detect_dma(false);
   if ((uint8) INVALID == sb.dma)
      return -1;

   /* may or may not exist */
   sb.dma16 = detect_dma(true);

   return 0;
}

/*
** Probe for an SB
*/
static int sb_probe(void)
{
   int retval;

   retval = parse_blaster_env();

   /* if environment parse failed, try brute force autodetection */
   if (-1 == retval)
      retval = sb_detect();

   /* no blaster found */
   if (-1 == retval)
   {
      thin_printf("thinlib.sb: no sound blaster found\n");
      return -1;
   }

   if (dsp_reset())
   {
      thin_printf("thinlib.sb: could not reset SB DSP: check BLASTER= variable\n");
      return -1;
   }

   sb.dsp_version = dsp_getversion();
   return 0;
}

/*
** Interrupt handler for 8/16-bit audio 
*/

static int sb_isr(void)
{
   uint32 address, offset;

   dma.count++;

   /* NOTE: this only works with 8-bit, as one-shot mode
   ** does not seem to work with 16-bit transfers
   */
   if (false == dma.autoinit)
   {
      dsp_write(DSP_DMA_DAC_8BIT);
      dsp_write(LOW_BYTE(sb.buf_size - 1));
      dsp_write(HIGH_BYTE(sb.buf_size - 1));
   }

   /* indicate we got the interrupt */
   inportb(dma.ackport);

   /* determine the current playback position */
   address = inportb(dma.addrport);
   address |= (inportb(dma.addrport) << 8);
   address -= dos.offset;

   if (address < sb.buf_size)
      offset = sb.buf_chunk;
   else
      offset = 0;

   sb.callback(sb.user_data, sb.buffer + offset, sb.buf_size);

   /* if we haven't enabled near pointers, we've written to a double
   ** buffer, so transfer it to low DOS memory area
   */
   if (0 == thinlib_nearptr)
      dosmemput(sb.buffer + offset, sb.buf_chunk, dos.bufaddr + offset);

   /* acknowledge interrupt was taken */
   if (sb.irq > 7)
      outportb(0xA0, 0x20);
   outportb(0x20, 0x20);

   return 0;
}
THIN_LOCKED_STATIC_FUNC(sb_isr)


/* install the SB ISR */
static void sb_setisr(void)
{
   THIN_DISABLE_INTS();

   thin_int_install(SB_IRQVEC(sb.irq), sb_isr);

   /* enable IRQ */
   thin_irq_enable(sb.irq);

   THIN_ENABLE_INTS();
}


static void sb_restoreisr(void)
{
   THIN_DISABLE_INTS();

   /* restore IRQ to previous state */
   thin_irq_restore(sb.irq);

   thin_int_remove(SB_IRQVEC(sb.irq));

   THIN_ENABLE_INTS();
}

/* allocate sound buffers */
static int sb_allocate_buffers(int buf_size)
{
   int double_bufsize;

   sb.buf_size = buf_size;

//   if (sb.format & SB_FORMAT_STEREO)
//      sb.buf_size *= 2;

   if (sb.format & SB_FORMAT_16BIT)
      sb.buf_chunk = sb.buf_size * sizeof(uint16);
   else
      sb.buf_chunk = sb.buf_size * sizeof(uint8);

   double_bufsize = 2 * sb.buf_chunk;

   dos.buffer.size = (double_bufsize + 15) >> 4;
   if (_go32_dpmi_allocate_dos_memory(&dos.buffer))
      return -1;

   /* calc linear address */
   dos.bufaddr = dos.buffer.rm_segment << 4;
   if (sb.format & SB_FORMAT_16BIT)
   {
      dos.page = (dos.bufaddr >> 16) & 0xFF;
      dos.offset = (dos.bufaddr >> 1) & 0xFFFF;
   }
   else
   {
      dos.page = (dos.bufaddr >> 16) & 0xFF;
      dos.offset = dos.bufaddr & 0xFFFF;
   }

   if (thinlib_nearptr)
   {
      sb.buffer = (uint8 *) THIN_PHYSICAL_ADDR(dos.bufaddr);
   }
   else
   {
      sb.buffer = malloc(double_bufsize);
      if (NULL == sb.buffer)
         return -1;
   }

   /* clear out the buffers */
   if (sb.format & SB_FORMAT_SIGNED)
      memset(sb.buffer, SILENCE_SIGNED, double_bufsize);
   else
      memset(sb.buffer, SILENCE_UNSIGNED, double_bufsize);

   if (0 == thinlib_nearptr)
      dosmemput(sb.buffer, double_bufsize, dos.bufaddr);

   return 0;
}

/* free buffers */
static void sb_free_buffers(void)
{
   sb.callback = NULL;

   _go32_dpmi_free_dos_memory(&dos.buffer);

   if (0 == thinlib_nearptr)
   {
      free(sb.buffer);
      sb.buffer = NULL;
   }

   sb.buffer = NULL;
}

/* get rid of all things SB */
void thin_sb_shutdown(void)
{
   if (true == sb.initialized)
   {
      sb.initialized = false;

      dsp_reset();

      sb_restoreisr();
      
      sb_free_buffers();
   }
}

/* initialize sound bastard */
int thin_sb_init(int *sample_rate, int *buf_size, int *format)
{
#define  CLAMP_RATE(in_rate, min_rate, max_rate) \
                    (in_rate < min_rate ? min_rate : \
                    (in_rate > max_rate ? max_rate : in_rate))
   
   /* don't init twice! */
   if (true == sb.initialized)
      return 0;

   /* lock variables, routines */
   THIN_LOCK_VAR(dma);
   THIN_LOCK_VAR(dos);
   THIN_LOCK_VAR(sb);
   THIN_LOCK_FUNC(sb_isr);

   memset(&sb, 0, sizeof(sb));
   
   if (sb_probe())
      return -1;

   /* try autoinit DMA first */
   dma.autoinit = true;
   sb.format = (uint8) *format;

   /* determine which SB model we have, and act accordingly */
   if (sb.dsp_version < DSP_VERSION_SB_15)
   {
      /* SB 1.0 */
      sb.sample_rate = CLAMP_RATE(*sample_rate, 4000, 22050);
      sb.format &= ~(SB_FORMAT_16BIT | SB_FORMAT_STEREO);
      dma.autoinit = false;
   }
   else if (sb.dsp_version < DSP_VERSION_SB_20)
   {
      /* SB 1.5 */
      sb.sample_rate = CLAMP_RATE(*sample_rate, 5000, 22050);
      sb.format &= ~(SB_FORMAT_16BIT | SB_FORMAT_STEREO);
   }
   else if (sb.dsp_version < DSP_VERSION_SB_PRO)
   {
      /* SB 2.0 */
      sb.sample_rate = CLAMP_RATE(*sample_rate, 5000, 44100);
      sb.format &= ~(SB_FORMAT_16BIT | SB_FORMAT_STEREO);
   }
   else if (sb.dsp_version < DSP_VERSION_SB16)
   {
      /* SB Pro */
      if (sb.format & SB_FORMAT_STEREO)
         sb.sample_rate = CLAMP_RATE(*sample_rate, 5000, 22050);
      else
         sb.sample_rate = CLAMP_RATE(*sample_rate, 5000, 44100);
      sb.format &= ~SB_FORMAT_16BIT;
   }
   else
   {
      /* SB 16 */
      sb.sample_rate = CLAMP_RATE(*sample_rate, 5000, 44100);
   }

   /* sanity check for 16-bit */
   if ((sb.format & SB_FORMAT_16BIT) && ((uint8) INVALID == sb.dma16))
   {
      sb.format &= ~SB_FORMAT_16BIT;
      thin_printf("thinlib.sb: 16-bit DMA channel not available, dropping to 8-bit\n");
   }

   /* clamp buffer size to something sane */
   if ((uint16) *buf_size > sb.sample_rate)
   {
      *buf_size = sb.sample_rate;
      thin_printf("thinlib.sb: buffer size too big, dropping to %d bytes\n", *buf_size);
   }

   /* allocate buffer / DOS memory */
   if (sb_allocate_buffers(*buf_size))
   {
      thin_printf("thinlib.sb: failed allocating sound buffers\n");
      return -1;
   }

   /* set the new IRQ vector! */
   sb_setisr();

   sb.initialized = true;

   /* return the actual values */
   *sample_rate = sb.sample_rate;
   *buf_size = sb.buf_size;
   *format = sb.format;

   return 0;
}

void thin_sb_stop(void)
{
   if (true == sb.initialized)
   {
      if (sb.format & SB_FORMAT_16BIT)
      {
         dsp_write(DSP_DMA_PAUSE_16BIT);  /* pause 16-bit DMA */
         dsp_write(DSP_DMA_STOP_8BIT);
         dsp_write(DSP_DMA_PAUSE_16BIT);
      }
      else
      {
         dsp_write(DSP_DMA_PAUSE_8BIT);  /* pause 8-bit DMA */
         dsp_write(DSP_SPEAKER_OFF);
      }
   }
}

/* return time constant for older sound bastards */
static uint8 get_time_constant(int rate)
{
   return ((65536 - (256000000L / rate)) / 256);
}

static void init_samplerate(int rate)
{
   if ((sb.format & SB_FORMAT_16BIT) || sb.dsp_version >= DSP_VERSION_SB16)
   {
      dsp_write(DSP_DMA_DAC_RATE);
      dsp_write(HIGH_BYTE(rate));
      dsp_write(LOW_BYTE(rate));
   }
   else
   {
      dsp_write(DSP_DMA_TIME_CONST);
      dsp_write(get_time_constant(rate));
   }
}

/* set the sample rate */
void thin_sb_setrate(int rate)
{
   if (sb.format & SB_FORMAT_16BIT)
   {
      dsp_write(DSP_DMA_PAUSE_16BIT);  /* pause 16-bit DMA */
      init_samplerate(rate);
      dsp_write(DSP_DMA_CONT_16BIT);   /* continue 16-bit DMA */
   }
   else
   {
      dsp_write(DSP_DMA_PAUSE_8BIT);   /* pause 8-bit DMA */
      init_samplerate(rate);
      dsp_write(DSP_DMA_CONT_8BIT);    /* continue 8-bit DMA */
   }

   sb.sample_rate = rate;
}

/* start SB DMA transfer */
static void start_transfer(void)
{
   uint8 dma_mode, start_command, mode_command;
   int dma_length;

   /* reset DMA count */
   dma.count = 0;

   dma_length = sb.buf_size * 2;

   if (true == dma.autoinit)
   {
      start_command = DSP_DMA_DAC_MODE;   /* autoinit DMA */
      dma_mode = DMA_AUTOINIT_MODE;
   }
   else
   {
      start_command = 0;
      dma_mode = DMA_ONESHOT_MODE;
   }

   /* things get a little bit nasty here, look out */
   if (sb.format & SB_FORMAT_16BIT)
   {
      uint8 dma_base = sb.dma16 - 4;

      dma_mode |= dma_base; 
      start_command |= DSP_DMA_START_16BIT;

      outportb(DMA_MASKPORT_16BIT, DMA_STOPMASK_BASE | dma_base);
      outportb(DMA_MODEPORT_16BIT, dma_mode);
      outportb(DMA_CLRPTRPORT_16BIT, 0x00);
      outportb(DMA_ADDRBASE_16BIT + (4 * dma_base), LOW_BYTE(dos.offset));
      outportb(DMA_ADDRBASE_16BIT + (4 * dma_base), HIGH_BYTE(dos.offset));
      outportb(DMA_COUNTBASE_16BIT + (4 * dma_base), LOW_BYTE(dma_length - 1));
      outportb(DMA_COUNTBASE_16BIT + (4 * dma_base), HIGH_BYTE(dma_length - 1));
      outportb(dma16_ports[dma_base], dos.page);
      outportb(DMA_MASKPORT_16BIT, DMA_STARTMASK_BASE | dma_base);

      dma.ackport = sb.baseio + DSP_DMA_ACK_16BIT;
      dma.addrport = DMA_ADDRBASE_16BIT + (4 * (sb.dma16 - 4));
   }
   else
   {
      dma_mode |= sb.dma;
      start_command |= DSP_DMA_START_8BIT;

      outportb(DMA_MASKPORT_8BIT, DMA_STOPMASK_BASE + sb.dma);
      outportb(DMA_MODEPORT_8BIT, dma_mode);
      outportb(DMA_CLRPTRPORT_8BIT, 0x00);
      outportb(DMA_ADDRBASE_8BIT + (2 * sb.dma), LOW_BYTE(dos.offset));
      outportb(DMA_ADDRBASE_8BIT + (2 * sb.dma), HIGH_BYTE(dos.offset));
      outportb(DMA_COUNTBASE_8BIT + (2 * sb.dma), LOW_BYTE(dma_length - 1));
      outportb(DMA_COUNTBASE_8BIT + (2 * sb.dma), HIGH_BYTE(dma_length - 1));
      outportb(dma8_ports[sb.dma], dos.page);
      outportb(DMA_MASKPORT_8BIT, DMA_STARTMASK_BASE + sb.dma);

      dma.ackport = sb.baseio + DSP_DMA_ACK_8BIT;
      dma.addrport = DMA_ADDRBASE_8BIT + (2 * sb.dma);
   }

   /* check signed/unsigned */
   if (sb.format & SB_FORMAT_SIGNED)
      mode_command = DSP_DMA_SIGNED;
   else
      mode_command = DSP_DMA_UNSIGNED;

   /* check stereo */
   if (sb.format & SB_FORMAT_STEREO)
      mode_command |= DSP_DMA_STEREO;
   else
      mode_command |= DSP_DMA_MONO;

   init_samplerate(sb.sample_rate);

   /* start things going */
   if ((sb.format & SB_FORMAT_16BIT) || sb.dsp_version >= DSP_VERSION_SB16)
   {
      dsp_write(start_command);
      dsp_write(mode_command);
      dsp_write(LOW_BYTE(sb.buf_size - 1));
      dsp_write(HIGH_BYTE(sb.buf_size - 1));
   }
   else
   {
      /* turn on speaker */
      dsp_write(DSP_SPEAKER_ON);

      if (true == dma.autoinit)
      {
         dsp_write(DSP_DMA_BLOCK_SIZE);  /* set buffer size */
         dsp_write(LOW_BYTE(sb.buf_size - 1));
         dsp_write(HIGH_BYTE(sb.buf_size - 1));

         if (sb.dsp_version < DSP_VERSION_SB_20)
            dsp_write(DSP_DMA_DAC_AI_8BIT); /* low speed autoinit */
         else
            dsp_write(DSP_DMA_DAC_HS_8BIT);
      }
      else
      {
         dsp_write(DSP_DMA_DAC_8BIT);
         dsp_write(LOW_BYTE(sb.buf_size - 1));
         dsp_write(HIGH_BYTE(sb.buf_size - 1));
      }
   }
}

/* TODO: this gets totally wacked when we change the timer rate!!! */
/* start playing the output buffer */
int thin_sb_start(sbmix_t fillbuf, void *user_data)
{
   clock_t count;
   int projected_dmacount;

   /* make sure we really should be here... */
   if (false == sb.initialized || NULL == fillbuf)
      return -1;

   /* stop any current processing */
   thin_sb_stop();

   /* set the callback routine */
   sb.callback = fillbuf;
   sb.user_data = user_data;

   /* calculate how many DMAs we should have in one second 
   ** and scale it down just a tad
   */
   projected_dmacount = (int) ((0.8 * sb.sample_rate) / sb.buf_size);
   if (projected_dmacount < 1)
      projected_dmacount = 1;

   /* get the transfer going, so we can ensure interrupts are firing */
   start_transfer();
   count = clock();
   while ((clock() - count) < CLOCKS_PER_SEC && dma.count < projected_dmacount)
      ; /* spin */

   if (dma.count < projected_dmacount)
   {
      if (true == dma.autoinit)
      {
         thin_printf("thinlib.sb: Autoinit DMA failed, trying one-shot mode.\n");
         dsp_reset();
         dma.autoinit = false;
         dma.count = 0;
         return (thin_sb_start(fillbuf, user_data));
      }
      else
      {
         thin_printf("thinlib.sb: One-shot DMA mode failed, sound will not be heard.\n");
         thin_printf("thinlib.sb: DSP version: %d.%d baseio: %X IRQ: %d DMA: %d High: %d\n",
                     sb.dsp_version >> 8, sb.dsp_version & 0xFF,
                     sb.baseio, sb.irq, sb.dma, sb.dma16);
         return -1;
      }
   }

   return 0;
}

/*
** $Log: $
*/
