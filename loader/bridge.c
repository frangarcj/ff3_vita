#include "bridge.h"
#include <limits.h>
#include <math.h>
#include <psp2/kernel/processmgr.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include "zlib.h"
#include <unicode/ucnv.h>
#include <unicode/ustring.h>

#define SAVE_FILENAME "ux0:/data/ff3"
#define OBB_FILE                                                               \
  "ux0:/data/ff3/main.20004.com.square_enix.android_googleplay.FFIII_GP.obb"

unsigned char *header = NULL;
int header_length = 0;

void decodeArray(unsigned char *bArr, int size, uint key) {
  for (int n = 0; n < size; n++) {
    key = key * 0x41c64e6d + 0x3039;
    bArr[n] = bArr[n] ^ (unsigned char)(key >> 0x18);
  }
}

static int getInt(unsigned char *bArr, int i) {
  return *(unsigned int *)(&bArr[i]);
}

unsigned char *gzipRead(unsigned char *bArr, int *bArr_length) {
  unsigned int readInt = __builtin_bswap32(getInt(bArr, 0));
  unsigned char *bArr2 = calloc(readInt, sizeof(unsigned char));
  unsigned char *bArr3 = &bArr[4];

  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  // setup "b" as the input and "c" as the compressed output
  infstream.avail_in = *bArr_length - 4; // size of input
  infstream.next_in = bArr3;             // input char array
  infstream.avail_out = readInt;         // size of output
  infstream.next_out = bArr2;            // output char array

  // the actual DE-compression work.
  inflateInit2(&infstream, MAX_WBITS | 16);
  inflate(&infstream, Z_FULL_FLUSH);
  inflateEnd(&infstream);

  *bArr_length = readInt;
  return bArr2;
}

unsigned char *m476a(char *str, int *file_length) {
  int i;

  unsigned char *bArr = header;
  if (bArr != NULL) {
    int a = getInt(bArr, 0);
    int i2 = 0;
    i = 0;
    while (a > i2) {
      int i3 = (i2 + a) / 2;
      int i4 = i3 * 12;
      int a2 = getInt(header, i4 + 4);
      int i5 = 0;
      for (int i6 = 0; i6 < strlen(str) && i5 == 0; i6++) {
        i5 = (header[a2 + i6] & 0xFF) - (str[i6] & 0xFF);
      }
      if (i5 == 0) {
        i5 = header[a2 + strlen(str)] & 0xFF;
      }
      if (i5 == 0) {
        i = i4 + 8;
        a = i3;
        i2 = a;
      } else if (i5 > 0) {
        a = i3;
      } else {
        i2 = i3 + 1;
      }
    }
  } else {
    i = 0;
  }
  if (i == 0) {
    return NULL;
  }

  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  int a3 = getInt(header, i + 0);
  fseek(fp, a3, SEEK_SET);

  *file_length = getInt(header, i + 4);
  unsigned char *bArr2 = malloc(*file_length);

  for (int i7 = 0; i7 < *file_length;
       i7 += fread(&bArr2[i7], sizeof(unsigned char), *file_length - i7, fp)) {
  }

  fclose(fp);

  decodeArray(bArr2, *file_length, a3 + 84861466u);

  unsigned char *a4 = gzipRead(bArr2, file_length);

  free(bArr2);

  return a4;
}

int readHeader() {
  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  fseek(fp, 0L, SEEK_END);
  int length = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  unsigned char bArr[16];

  for (int i = 0; i < 16;
       i += fread(&bArr[i], sizeof(unsigned char), 16 - i, fp)) {
  }

  decodeArray(bArr, 16, 84861466u);

  if (getInt(bArr, 0) != 826495553) {
    printf("initFileTable: Header Error\n");
    return 0;
  } else if (length != getInt(bArr, 4)) {
    printf("initFileTable: Size Error\n");
    return 0;
  } else {
    unsigned int a2 = getInt(bArr, 8);
    header_length = getInt(bArr, 12);
    header = malloc(header_length);

    fseek(fp, (long)(a2 - 16), SEEK_CUR);

    for (int i = 0; i < header_length;
         i += fread(&header[i], sizeof(unsigned char), header_length - i, fp)) {
    }

    decodeArray(header, header_length, a2 + 84861466u);

    unsigned char *header2 = gzipRead(header, &header_length);

    free(header);

    header = header2;

    fclose(fp);
  }
  return 1;
}

void toUtf8(const char *src, size_t length, char *dst, const char *src_encoding,
            size_t *dst_length_p) {

  UErrorCode status = U_ZERO_ERROR;
  UConverter *conv;
  int32_t len;

  char *temp = malloc(length * 2);
  u_setDataDirectory("app0:/");
  printf("%s\n", u_getDataDirectory());
  conv = ucnv_open(src_encoding, &status);

  len = ucnv_toUChars(conv, temp, length * 2, src, length, &status);
  ucnv_close(conv);

  conv = ucnv_open("utf-8", &status);
  *dst_length_p = ucnv_fromUChars(conv, dst, length * 2, temp, len, &status);
  ucnv_close(conv);

  free(temp);
}

unsigned char *decodeString(unsigned char *bArr, int *bArr_length) {

  int i = 5;
  if (i >= 6) {
    return bArr;
  }

  unsigned char *bArr3 = malloc(*bArr_length * 2);
  int i4 = (bArr[8] & 0xFF) | ((bArr[9] & 0xFF) << 8) |
           ((bArr[10] & 0xFF) << 16) | ((bArr[11] & 0xFF) << 24);
  int i5 = (i4 * 12) + 16;

  memcpy(bArr3, bArr, i5);

  int i6 = i5;
  int i7 = 0;
  int i8 = 16;
  int i9 = 16;

  while (i7 < i4) {
    int i10 = i8 + 8;
    bArr3[i10 + 0] = (unsigned char)(i6 >> 0);
    bArr3[i10 + 1] = (unsigned char)(i6 >> 8);
    bArr3[i10 + 2] = (unsigned char)(i6 >> 16);
    bArr3[i10 + 3] = (unsigned char)(i6 >> 24);
    unsigned char b = bArr[i9 + 4];
    int i11 = i9 + 8;
    int i12 = ((bArr[i11 + 3] & 0xFF) << 24) | (bArr[i11 + 0] & 0xFF) |
              ((bArr[i11 + 1] & 0xFF) << 8) | ((bArr[i11 + 2] & 0xFF) << 16);
    int i13 = i6;
    for (int i14 = 0; i14 < b; i14++) {
      int i15 = 0;
      while (bArr[i12 + i15] != 0) {
        i15++;
      }
      int newLength = 0;
      toUtf8(&bArr[i12], i15, &bArr3[i13],
             i == 0 ? "shift_jis" : "windows-1252", &newLength);

      bArr3[newLength + i13] = 0;

      i12 += i15 + 1;
      i13 += newLength + 1;
    }
    i6 = i13 + 1;
    bArr3[i13] = 0;
    i9 += 12;
    i8 += 12;
    i7++;
  }

  unsigned char *bArr4 = malloc(i6);
  memcpy(bArr4, bArr3, i6);
  free(bArr3);
  *bArr_length = i6;
  return bArr4;
}

jni_bytearray *loadFile(char *str) {
  char *lang[] = {"ja", "en",    "fr",    "de", "it",
                  "es", "zh_CN", "zh_TW", "ko", "th"};

  char *substring = strrchr(str, 46);

  substring = substring == NULL ? str : substring;
  char temp_path[512];
  int file_length;
  sprintf(temp_path, "%s.lproj/%s", lang[5], str);
  unsigned char *a = m476a(temp_path, &file_length);
  if (a == NULL) {
    sprintf(temp_path, "files/%s", str);
    a = m476a(temp_path, &file_length);
  }
  if (a == NULL) {
    return NULL;
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  if (strcmp(substring, ".msd") || str[0] == 'e')
    return result;
  if (str[0] == 'n' && str[1] == 'o' && str[2] == 'a')
    return result;

  unsigned char *b = decodeString(a, &file_length);

  free(a);

  result->elements = b;
  result->size = file_length;

  return result;
}

jni_bytearray *loadRawFile(char *str) {

  int file_length;
  unsigned char *a = m476a(str, &file_length);
  if(strstr(str,"sound")) //TODO: FIX SOUND
    return NULL;

  if (a == NULL) {
    return NULL;
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  return result;
}

jni_bytearray *getSaveFileName() {

  char *buffer = SAVE_FILENAME;
  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = malloc(strlen(buffer) + 1);
  // Sets the value
  strcpy((char *)result->elements, buffer);
  result->size = strlen(buffer) + 1;

  return result;
}

void createSaveFile(size_t size) {
  char *buffer = malloc(size);
  FILE *fd = fopen(SAVE_FILENAME "/save.bin", "wb");
  fwrite(buffer, sizeof(char), size, fd);
  fclose(fd);
  free(buffer);
}

uint64_t j3 = 0;

uint64_t getCurrentFrame(uint64_t j) {

  while (1) {
    uint64_t j2 = sceKernelGetProcessTimeWide() * 3 / 100000;
    if (j2 != j3) {
      j3 = j2;
      return j2;
    }
    sceKernelDelayThread(1000);
  }
}

#define RGBA8(r, g, b, a)                                                      \
  ((((a)&0xFF) << 24) | (((b)&0xFF) << 16) | (((g)&0xFF) << 8) |               \
   (((r)&0xFF) << 0))

jni_intarray *loadTexture(jni_bytearray *bArr) {

  jni_intarray *texture = malloc(sizeof(jni_intarray));

  int x, y, channels_in_file;
  unsigned char *temp = stbi_load_from_memory(bArr->elements, bArr->size, &x,
                                              &y, &channels_in_file, 4);

  texture->size = x * y + 2;
  texture->elements = malloc(texture->size * sizeof(int));
  texture->elements[0] = x;
  texture->elements[1] = y;

  for (int n = 0; n < y; n++) {
    for (int m = 0; m < x; m++) {
      unsigned char *color = (unsigned char *)&(((uint32_t *)temp)[n * x + m]);
      texture->elements[2 + n * x + m] =
          RGBA8(color[2], color[1], color[0], color[3]);
    }
  }

  free(temp);

  return texture;
}

int isDeviceAndroidTV() { return 1; }

stbtt_fontinfo *info = NULL;
unsigned char *fontBuffer = NULL;

void initFont() {

  long size;

  if (info != NULL)
    return;

  FILE *fontFile = fopen("ux0:/data/ff3/Roboto-Bold.ttf", "rb");
  fseek(fontFile, 0, SEEK_END);
  size = ftell(fontFile);       /* how long is the file ? */
  fseek(fontFile, 0, SEEK_SET); /* reset */

  fontBuffer = malloc(size);

  fread(fontBuffer, size, 1, fontFile);
  fclose(fontFile);

  info = malloc(sizeof(stbtt_fontinfo));

  /* prepare font */
  if (!stbtt_InitFont(info, fontBuffer, 0)) {
    printf("failed\n");
  }
}

static inline uint32_t utf8_decode_unsafe_2(const char *data) {
  uint32_t codepoint;

  codepoint = ((data[0] & 0x1F) << 6);
  codepoint |= (data[1] & 0x3F);

  return codepoint;
}

jni_intarray *drawFont(char *word, int size, int i2, int i3) {
  printf("%s %d %d %d\n", word, size, i2, i3);

  initFont();

  jni_intarray *texture = malloc(sizeof(jni_intarray));
  texture->size = size * size + 5;
  texture->elements = malloc(texture->size * sizeof(int));

  int b_w = size; /* bitmap width */
  int b_h = size; /* bitmap height */

  /* create a bitmap for the phrase */
  unsigned char *bitmap = calloc(b_w * b_h, sizeof(unsigned char));

  /* calculate font scaling */
  float scale = stbtt_ScaleForPixelHeight(info, size);

  int x = 0;

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);

  int i = 0;
  while (word[i]) {
    i++;
  }

  int codepoint = i == 2 ? utf8_decode_unsafe_2(word) : word[0];

  int ax;
  int lsb;
  stbtt_GetCodepointHMetrics(info, codepoint, &ax, &lsb);

  if (codepoint == 32) {
    texture->elements[0] = roundf(ax * scale);
    return texture;
  }

  /* get bounding box for character (may be offset to account for chars that
   * dip above or below the line */
  int c_x1, c_y1, c_x2, c_y2;
  stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &c_x1, &c_y1,
                              &c_x2, &c_y2);

  /* compute y (different characters have different heights */
  int y = roundf(ascent * scale) + c_y1;

  /* render character (stride and offset is important here) */
  int byteOffset = x + roundf(lsb * scale) + (y * b_w);

  stbtt_MakeCodepointBitmap(info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1,
                            b_w, scale, scale, codepoint);

  texture->elements[0] = (c_x2 - c_x1 + x + roundf(lsb * scale));
  texture->elements[1] = 0;
  texture->elements[2] = 0;

  for (int n = 0; n < size * size; n++) {
    texture->elements[5 + n] =
        RGBA8(bitmap[n], bitmap[n], bitmap[n], bitmap[n]);
  }

  return texture;
}
