#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/types.h>

#include <stdlib.h>

#include "gameboy_printer_protocol.h"
#include "gbp_pkt.h"
#include "gbp_tiles.h"
#include "ecma48.h"


/* The official name of this program (e.g., no 'g' prefix).  */
#define PROGRAM_NAME "gpbdecoder"

//static bool verbose_flag = false;

char * ifilename = NULL;
char * ofilename = NULL;
FILE * ifilePtr = NULL;
FILE * ofilePtr = NULL;


void gbpdecoder_gotByte(const uint8_t byte);


uint8_t pktCounter = 0; // Dev Varible
//////
gbp_pkt_t gbp_pktBuff = {GBP_REC_NONE, 0};
uint8_t gbp_pktbuff[GBP_PKT_PAYLOAD_BUFF_SIZE_IN_BYTE] = {0};
uint8_t gbp_pktbuffSize = 0;
gbp_pkt_tileAcc_t tileBuff = {0};
//////

/*******************************************************************************
 * Test Vectors Variables
*******************************************************************************/
const uint8_t testVector[] = {
  //#include "2020-08-02_GameboyPocketCameraJP.txt" // Single Image
  //#include "2020-08-02_PokemonSpeciallPicachuEdition.txt" // Mult-page Image
  #include "./test/2020-08-10_Pokemon_trading_card_compressiontest.txt" // Compression
};



/*******************************************************************************
 * Utilites
*******************************************************************************/

const char *gbpCommand_toStr(int val)
{
  switch (val)
  {
    case GBP_COMMAND_INIT    : return "INIT";
    case GBP_COMMAND_PRINT   : return "PRNT";
    case GBP_COMMAND_DATA    : return "DATA";
    case GBP_COMMAND_BREAK   : return "BREK";
    case GBP_COMMAND_INQUIRY : return "INQY";
    default: return "?";
  }
}












/*******************************************************************************
 * Main Test Routine
*******************************************************************************/
int
main (int argc, char **argv)
{
  (void) argc;
  (void) argv;

  int c;
  static struct option const long_options[] =
  {
    /* These options set a flag. */
    //{"verbose", no_argument, &verbose_flag, 1},
    //{"brief",   no_argument, &verbose_flag, 0},
    /* These options don’t set a flag.
        We distinguish them by their indices. */
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}
  };

  while ((c = getopt_long (argc, argv, "o:i:", long_options, NULL))
         != -1)
  {
    switch (c)
    {
        case 'i':
          ifilename = optarg;
          break;

        case 'o':
          ofilename = optarg;
          break;
    }
  }

  if (ifilename)
  {
    ifilePtr = fopen(ifilename, "r+");
    if (ifilePtr)
    {
      printf ("file input `%s' open\n", ifilename);
    }
  }
  if (ifilePtr == NULL)
  {
    ifilePtr = stdin;
    printf ("file input stdin\n");
  }

  if (ofilename)
  {
    ofilePtr = fopen(ofilename, "w+");
    if (ofilePtr)
    {
      printf ("file output `%s' open\n", ofilename);
    }
  }

  gbp_pkt_init(&gbp_pktBuff);

  char ch = 0;
  bool skipLine = false;
  int  lowNibFound = 0;
  uint8_t byte = 0;
  unsigned int bytec = 0;
  while ((ch = fgetc(ifilePtr)) != EOF)
  {
    // Skip Comments
    if (ch == '/')
    {
      skipLine = true;
      continue;
    }
    else if (skipLine)
    {
      if (ch == '\n')
      {
        skipLine = false;
      }
      continue;
    }

    // Parse Nib
    char nib = -1;
    if (('0' <= ch) && (ch <= '9'))
    {
      nib = ch - '0';
    }
    else if (('a' <= ch) && (ch <= 'f'))
    {
      nib = ch - 'a' + 10;
    }
    else if (('A' <= ch) && (ch <= 'F'))
    {
      nib = ch - 'A' + 10;
    }

    /* Parse As Byte */
    bool byteFound = false;
    // Hex Parse Edge Cases
    if (lowNibFound)
    {
      // '0x' found. Ignore
      if ((byte == 0) && (ch == 'x'))
        lowNibFound = false;
      // Not a hex digit pair. Ignore
      if (nib == -1)
        lowNibFound = false;
    }
    // Hex Byte Parsing
    if (nib != -1)
    {
      if (!lowNibFound)
      {
        lowNibFound = true;
        byte = nib << 4;
      }
      else
      {
        lowNibFound = false;
        byte |= nib << 0;
        byteFound = true;
      }
    }

    // Byte Was Found
    if (byteFound)
    {
      bytec++;
      //printf("0x%02X %s", byte, ((bytec%16) == 0) ? "\r\n" : "");
      // Decode
      gbpdecoder_gotByte(byte);
    }
  }

  return 0;
}


void gbpdecoder_gotByte(const uint8_t byte)
{
  if (gbp_pkt_processByte(&gbp_pktBuff, byte, gbp_pktbuff, &gbp_pktbuffSize, sizeof(gbp_pktbuff)))
  {
    if (gbp_pktBuff.received == GBP_REC_GOT_PACKET)
    {
      pktCounter++;
#if 0
      printf("// %s | compression: %1u, dlength: %3u, printerID: 0x%02X, status: %u | %d | ",
          gbpCommand_toStr(gbp_pktBuff.command),
          (unsigned) gbp_pktBuff.compression,
          (unsigned) gbp_pktBuff.dataLength,
          (unsigned) gbp_pktBuff.printerID,
          (unsigned) gbp_pktBuff.status,
          (unsigned) pktCounter
        );
      for (int i = 0 ; i < gbp_pktbuffSize ; i++)
      {
        printf("%02X ", gbp_pktbuff[i]);
      }
      printf("\r\n");
#endif
    }
    else
    {
      // Support compression payload
      while (gbp_pkt_decompressor(&gbp_pktBuff, gbp_pktbuff, gbp_pktbuffSize, &tileBuff))
      {
        if (gbp_pkt_tileAccu_tileReadyCheck(&tileBuff))
        {
          // Got tile
#if 0
          // Output Tile As Hex For Debugging purpose
          for (int i = 0 ; i < GBP_TILE_SIZE_IN_BYTE ; i++)
          {
            printf("%02X ", tileBuff.tile[i]);
          }
          printf("\r\n");
#endif
          static gbp_tile_t gbp_tiles = {0};
          if (gbp_tiles_line_decoder(&gbp_tiles, tileBuff.tile))
          {
            for (uint8_t j = 0; j < GBP_TILE_PIXEL_HEIGHT; j++)
            {
              for (uint8_t i = 0; i < GBP_TILE_PIXEL_WIDTH * GBP_TILES_PER_LINE; i++)
              {
                int pixel = gbp_tiles.bmpLineBuffer[j][i];
                switch (pixel)
                {
                  case 0: printf(ECMA48_SGR_BACKGROUND_COLOR_24BIT(0,0,0) " " ECMA48_CSI_SGR(0)); break;
                  case 1: printf(ECMA48_SGR_BACKGROUND_COLOR_24BIT(64,64,64) " " ECMA48_CSI_SGR(0)); break;
                  case 2: printf(ECMA48_SGR_BACKGROUND_COLOR_24BIT(130,130,130) " " ECMA48_CSI_SGR(0)); break;
                  case 3: printf(ECMA48_SGR_BACKGROUND_COLOR_24BIT(255,255,255) " " ECMA48_CSI_SGR(0)); break;
                }
              }
              printf("\r\n");
            }
          }
        }
      }
    }
  }
}

