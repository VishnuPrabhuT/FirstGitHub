#include <linux/ioctl.h>

#define KYOUKO3_MAJOR 500
#define KYOUKO3_MINOR 127

#define PCI_VENDOR_ID_CCORSI 0x1234
#define PCI_DEVICE_ID_CCORSI_KYOUKO3 0x1113

#define CONTROL_SIZE 65536 

#define FIFO_ENTRIES 1024

// registers
#define DeviceVRAM 0x0020

#define FifoStart 0x1020
#define FifoEnd 0x1024
#define FifoHead 0x4010
#define FifoTail 0x4014

#define FrameColumns 0x8000
#define FrameRows 0x8004
#define FrameRowPitch 0x8008
#define FramePixelFormat 0x800C
#define FrameStartAddress 0x8010

#define EncoderWidth 0x9000
#define EncoderHeight 0x9004
#define EncoderOffsetX 0x9008
#define EncoderOffsetY 0x900C
#define EncoderFrame 0x9010

#define ConfigAcceleration 0x1010
#define ConfigModeSet 0x1008

#define DrawClearColor4fBlue 0x5100
#define DrawClearColor4fGreen 0x5104
#define DrawClearColor4fRed 0x5108
#define DrawClearColor4fAlpha 0x510C

#define RasterClear 0x3008
#define RasterFlush 0x3FFC

// ioctl commands
#define VMODE      _IOW (0xCC, 0, unsigned long)
#define BIND_DMA   _IOW (0xCC, 1, unsigned long)
#define START_DMA  _IOWR(0xCC, 2, unsigned long)
#define FIFO_QUEUE _IOWR(0xCC, 3, unsigned long)
#define FIFO_FLUSH _IO  (0xCC, 4)
#define UNBIND_DMA _IOW (0xCC, 5, unsigned long)

#define GRAPHICS_OFF 0
#define GRAPHICS_ON 1
