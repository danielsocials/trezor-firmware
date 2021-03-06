/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "bignum.h"
#include "bitmaps.h"
#include "buttons.h"
#include "common.h"
#include "config.h"
#include "gettext.h"
#include "layout2.h"
#include "memory.h"
#include "memzero.h"
#include "nem2.h"
#include "oled.h"
#include "prompt.h"
#include "qrcodegen.h"
#include "secp256k1.h"
#include "sys.h"
#include "timer.h"
#include "util.h"

uint8_t Disp_buffer[DISP_BUFSIZE];
uint8_t s_usPower_Button_Status = POWER_BUTTON_UP;

static uint16_t s_usCurrentCount;
static uint16_t s_uiShowLength;

/* Screen timeout */
uint32_t system_millis_lock_start = 0;

#define BLE_NAME "Ble name:"
#define BLE_MAC "Ble mac:"
#define BLE_VER "Ble version:"

#define USB_LABLE " Lable:"
#define USB_SN "SN:"
#define USB_VER "USB version:"
#define APP_FINGERPRINT "fingerprint:"

#define DEFAULTLABE "Bixin"
#define DEFAULSN "20200304"

#if !BITCOIN_ONLY

static const char *slip44_extras(uint32_t coin_type) {
  if ((coin_type & 0x80000000) == 0) {
    return 0;
  }
  switch (coin_type & 0x7fffffff) {
    case 40:
      return "EXP";  // Expanse
    case 43:
      return "NEM";  // NEM
    case 60:
      return "ETH";  // Ethereum Mainnet
    case 61:
      return "ETC";  // Ethereum Classic Mainnet
    case 108:
      return "UBQ";  // UBIQ
    case 137:
      return "RSK";  // Rootstock Mainnet
    case 37310:
      return "tRSK";  // Rootstock Testnet
  }
  return 0;
}

#endif

#define BIP32_MAX_LAST_ELEMENT 1000000

static const char *address_n_str(const uint32_t *address_n,
                                 size_t address_n_count,
                                 bool address_is_account) {
  if (address_n_count > 8) {
    return _("Unknown long path");
  }
  if (address_n_count == 0) {
    return _("Path: m");
  }

  // known BIP44/49 path
  static char path[100];
  if (address_n_count == 5 &&
      (address_n[0] == (0x80000000 + 44) || address_n[0] == (0x80000000 + 49) ||
       address_n[0] == (0x80000000 + 84)) &&
      (address_n[1] & 0x80000000) && (address_n[2] & 0x80000000) &&
      (address_n[3] <= 1) && (address_n[4] <= BIP32_MAX_LAST_ELEMENT)) {
    bool native_segwit = (address_n[0] == (0x80000000 + 84));
    bool p2sh_segwit = (address_n[0] == (0x80000000 + 49));
    bool legacy = false;
    const CoinInfo *coin = coinBySlip44(address_n[1]);
    const char *abbr = 0;
    if (native_segwit) {
      if (coin && coin->has_segwit && coin->bech32_prefix) {
        abbr = coin->coin_shortcut + 1;
      }
    } else if (p2sh_segwit) {
      if (coin && coin->has_segwit) {
        abbr = coin->coin_shortcut + 1;
      }
    } else {
      if (coin) {
        if (coin->has_segwit) {
          legacy = true;
        }
        abbr = coin->coin_shortcut + 1;
#if !BITCOIN_ONLY
      } else {
        abbr = slip44_extras(address_n[1]);
#endif
      }
    }
    const uint32_t accnum = address_is_account
                                ? ((address_n[4] & 0x7fffffff) + 1)
                                : (address_n[2] & 0x7fffffff) + 1;
    if (abbr && accnum < 100) {
      memzero(path, sizeof(path));
      strlcpy(path, abbr, sizeof(path));
      // TODO: how to name accounts?
      // currently we have "legacy account", "account" and "segwit account"
      // for BIP44/P2PKH, BIP49/P2SH-P2WPKH and BIP84/P2WPKH respectivelly
      if (legacy) {
        strlcat(path, " legacy", sizeof(path));
      }
      if (native_segwit) {
        strlcat(path, " segwit", sizeof(path));
      }
      if (address_is_account) {
        strlcat(path, " address #", sizeof(path));
      } else {
        strlcat(path, " account #", sizeof(path));
      }
      char acc[3] = {0};
      memzero(acc, sizeof(acc));
      if (accnum < 10) {
        acc[0] = '0' + accnum;
      } else {
        acc[0] = '0' + (accnum / 10);
        acc[1] = '0' + (accnum % 10);
      }
      strlcat(path, acc, sizeof(path));
      return path;
    }
  }

  //                  "Path: m"    /    i   '
  static char address_str[7 + 8 * (1 + 10 + 1) + 1];
  char *c = address_str + sizeof(address_str) - 1;

  *c = 0;
  c--;

  for (int n = (int)address_n_count - 1; n >= 0; n--) {
    uint32_t i = address_n[n];
    if (i & 0x80000000) {
      *c = '\'';
      c--;
    }
    i = i & 0x7fffffff;
    do {
      *c = '0' + (i % 10);
      c--;
      i /= 10;
    } while (i > 0);
    *c = '/';
    c--;
  }
  *c = 'm';
  c--;
  *c = ' ';
  c--;
  *c = ':';
  c--;
  *c = 'h';
  c--;
  *c = 't';
  c--;
  *c = 'a';
  c--;
  *c = 'P';

  return c;
}

// split longer string into 4 rows, rowlen chars each
const char **split_message(const uint8_t *msg, uint32_t len, uint32_t rowlen) {
  static char str[4][32 + 1];
  if (rowlen > 32) {
    rowlen = 32;
  }
  memset(Disp_buffer, 0x00, DISP_BUFSIZE);
  if (len < DISP_BUFSIZE) {
    memcpy(Disp_buffer, msg, len);
  }

  s_usCurrentCount = 0;
  s_uiShowLength = len;

  memzero(str, sizeof(str));
  strlcpy(str[0], (char *)msg, rowlen + 1);
  if (len > rowlen) {
    strlcpy(str[1], (char *)msg + rowlen, rowlen + 1);
  }
  if (len > rowlen * 2) {
    strlcpy(str[2], (char *)msg + rowlen * 2, rowlen + 1);
  }
  if (len > rowlen * 3) {
    strlcpy(str[3], (char *)msg + rowlen * 3, rowlen + 1);
  }
  //  if (len > rowlen * 4) {
  //    str[3][rowlen - 1] = '.';
  //    str[3][rowlen - 2] = '.';
  //    str[3][rowlen - 3] = '.';
  //  }
  static const char *ret[4] = {str[0], str[1], str[2], str[3]};
  return ret;
}

const char **split_message_hex(const uint8_t *msg, uint32_t len) {
  char hex[1024 * 2 + 1] = {0};
  memzero(hex, sizeof(hex));
  uint32_t size = len;
  if (len > 1024) {
    size = 1024;
  }
  data2hex(msg, size, hex);
  //  if (len > 32) {
  //    hex[63] = '.';
  //    hex[62] = '.';
  //  }
  return split_message((const uint8_t *)hex, size * 2, 16);
}

void *layoutLast = layoutHome;

void layoutDialogSwipe(const BITMAP *icon, const char *btnNo,
                       const char *btnYes, const char *desc, const char *line1,
                       const char *line2, const char *line3, const char *line4,
                       const char *line5, const char *line6) {
  layoutLast = layoutDialogSwipe;
  layoutSwipe();
  layoutDialog(icon, btnNo, btnYes, desc, line1, line2, line3, line4, line5,
               line6);
}

void layoutProgressSwipe(const char *desc, int permil) {
  if (layoutLast == layoutProgressSwipe) {
    oledClear();
  } else {
    layoutLast = layoutProgressSwipe;
    layoutSwipe();
  }
  layoutProgress(desc, permil);
}

void layoutScreensaver(void) {
  layoutLast = layoutScreensaver;
  oledClear();
  oledRefresh();
}

void vlayoutLogo(void) {
  oledDrawBitmap(0, 16, &bmp_logo);
  if (!config_isInitialized()) {
    vDisp_PromptInfo(DISP_NOT_ACTIVE, false);
  }
  oledclearLine(0);
  oledclearLine(1);
  vDisp_PromptInfo(DISP_BLE_NAME, false);
  oledRefresh();
}

void layoutHome(void) {
  if (layoutLast == layoutHome || layoutLast == layoutScreensaver) {
    oledClear();
  } else {
    layoutSwipe();
  }
  layoutLast = layoutHome;
  char label[MAX_LABEL_LEN + 1] = _("Go to trezor.io/start");
  if (config_isInitialized()) {
    config_getLabel(label, sizeof(label));
  }

  uint8_t homescreen[HOMESCREEN_SIZE] = {0};
  if (config_getHomescreen(homescreen, sizeof(homescreen))) {
    BITMAP b = {0};
    b.width = 128;
    b.height = 64;
    b.data = homescreen;
    oledDrawBitmap(0, 0, &b);
  } else {
    vlayoutLogo();
  }

  bool no_backup = false;
  bool unfinished_backup = false;
  bool needs_backup = false;
  config_getNoBackup(&no_backup);
  config_getUnfinishedBackup(&unfinished_backup);
  config_getNeedsBackup(&needs_backup);
  if (no_backup) {
    oledBox(0, OLED_HEIGHT - 8, 127, 8, false);
    oledclearLine(6);
    oledclearLine(7);
    oledDrawStringCenter(OLED_WIDTH / 2, OLED_HEIGHT - 8, "SEEDLESS",
                         FONT_STANDARD);
  } else if (unfinished_backup) {
    oledBox(0, OLED_HEIGHT - 8, 127, 8, false);
    oledclearLine(6);
    oledclearLine(7);
    oledDrawStringCenter(OLED_WIDTH / 2, OLED_HEIGHT - 8, "BACKUP FAILED!",
                         FONT_STANDARD);
  } else if (needs_backup) {
    oledBox(0, OLED_HEIGHT - 8, 127, 8, false);
    oledclearLine(6);
    oledclearLine(7);
    oledDrawStringCenter(OLED_WIDTH / 2, OLED_HEIGHT - 8, "NEEDS BACKUP!",
                         FONT_STANDARD);
  }
  oledRefresh();

  // Reset lock screen timeout
  system_millis_lock_start = timer_ms();
}

static void render_address_dialog(const CoinInfo *coin, const char *address,
                                  const char *line1, const char *line2,
                                  const char *extra_line) {
  if (coin && coin->cashaddr_prefix) {
    /* If this is a cashaddr address, remove the prefix from the
     * string presented to the user
     */
    int prefix_len = strlen(coin->cashaddr_prefix);
    if (strncmp(address, coin->cashaddr_prefix, prefix_len) == 0 &&
        address[prefix_len] == ':') {
      address += prefix_len + 1;
    }
  }
  int addrlen = strlen(address);
  int numlines = addrlen <= 42 ? 2 : 3;
  int linelen = (addrlen - 1) / numlines + 1;
  if (linelen > 21) {
    linelen = 21;
  }
  const char **str = split_message((const uint8_t *)address, addrlen, linelen);
  layoutLast = layoutDialogSwipe;
  layoutSwipe();
  oledClear();
  oledDrawBitmap(0, 0, &bmp_icon_question);
  oledDrawString(20, 0 * 9, line1, FONT_STANDARD);
  oledDrawString(20, 1 * 9, line2, FONT_STANDARD);
  int left = linelen > 18 ? 0 : 20;
  oledDrawString(left, 2 * 9, str[0], FONT_FIXED);
  oledDrawString(left, 3 * 9, str[1], FONT_FIXED);
  oledDrawString(left, 4 * 9, str[2], FONT_FIXED);
  oledDrawString(left, 5 * 9, str[3], FONT_FIXED);
  if (!str[3][0]) {
    if (extra_line) {
      oledDrawString(0, 5 * 9, extra_line, FONT_STANDARD);
    } else {
      oledHLine(OLED_HEIGHT - 13);
    }
  }
  layoutButtonNo(_("Cancel"), &bmp_btn_cancel);
  layoutButtonYes(_("Confirm"), &bmp_btn_confirm);
  oledRefresh();
}

void layoutConfirmOutput(const CoinInfo *coin, const TxOutputType *out) {
  char str_out[32 + 3] = {0};
  bn_format_uint64(out->amount, NULL, coin->coin_shortcut, coin->decimals, 0,
                   false, str_out, sizeof(str_out) - 3);
  strlcat(str_out, " to", sizeof(str_out));
  const char *address = out->address;
  const char *extra_line =
      (out->address_n_count > 0)
          ? address_n_str(out->address_n, out->address_n_count, false)
          : 0;
  render_address_dialog(coin, address, _("Confirm sending"), str_out,
                        extra_line);
}

void layoutConfirmOmni(const uint8_t *data, uint32_t size) {
  const char *desc = NULL;
  char str_out[32] = {0};
  uint32_t tx_type = 0, currency = 0;
  REVERSE32(*(const uint32_t *)(data + 4), tx_type);
  if (tx_type == 0x00000000 && size == 20) {  // OMNI simple send
    desc = _("Simple send of ");
    REVERSE32(*(const uint32_t *)(data + 8), currency);
    const char *suffix = " UNKN";
    bool divisible = false;
    switch (currency) {
      case 1:
        suffix = " OMNI";
        divisible = true;
        break;
      case 2:
        suffix = " tOMNI";
        divisible = true;
        break;
      case 3:
        suffix = " MAID";
        divisible = false;
        break;
      case 31:
        suffix = " USDT";
        divisible = true;
        break;
    }
    uint64_t amount_be = 0, amount = 0;
    memcpy(&amount_be, data + 12, sizeof(uint64_t));
    REVERSE64(amount_be, amount);
    bn_format_uint64(amount, NULL, suffix, divisible ? 8 : 0, 0, false, str_out,
                     sizeof(str_out));
  } else {
    desc = _("Unknown transaction");
    str_out[0] = 0;
  }
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Confirm OMNI Transaction:"), NULL, desc, NULL, str_out,
                    NULL);
}

static bool is_valid_ascii(const uint8_t *data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    if (data[i] < ' ' || data[i] > '~') {
      return false;
    }
  }
  return true;
}

void layoutConfirmOpReturn(const uint8_t *data, uint32_t size) {
  const char **str = NULL;
  if (!is_valid_ascii(data, size)) {
    str = split_message_hex(data, size);
  } else {
    str = split_message(data, size, 20);
  }
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Confirm OP_RETURN:"), str[0], str[1], str[2], str[3],
                    NULL);
}

void layoutConfirmTx(const CoinInfo *coin, uint64_t amount_out,
                     uint64_t amount_fee) {
  char str_out[32] = {0}, str_fee[32] = {0};
  bn_format_uint64(amount_out, NULL, coin->coin_shortcut, coin->decimals, 0,
                   false, str_out, sizeof(str_out));
  bn_format_uint64(amount_fee, NULL, coin->coin_shortcut, coin->decimals, 0,
                   false, str_fee, sizeof(str_fee));
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Really send"), str_out, _("from your wallet?"),
                    _("Fee included:"), str_fee, NULL);
}

void layoutFeeOverThreshold(const CoinInfo *coin, uint64_t fee) {
  char str_fee[32] = {0};
  bn_format_uint64(fee, NULL, coin->coin_shortcut, coin->decimals, 0, false,
                   str_fee, sizeof(str_fee));
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), NULL,
                    _("Fee"), str_fee, _("is unexpectedly high."), NULL,
                    _("Send anyway?"), NULL);
}

void layoutSignMessage(const uint8_t *msg, uint32_t len) {
  const char **str = NULL;
  if (!is_valid_ascii(msg, len)) {
    str = split_message_hex(msg, len);
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"),
                      _("Sign binary message?"), str[0], str[1], str[2], str[3],
                      NULL, NULL);
  } else {
    str = split_message(msg, len, 20);
    layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"),
                      _("Sign message?"), str[0], str[1], str[2], str[3], NULL,
                      NULL);
  }
}

void layoutVerifyMessage(const uint8_t *msg, uint32_t len) {
  const char **str = NULL;
  if (!is_valid_ascii(msg, len)) {
    str = split_message_hex(msg, len);
    layoutDialogSwipe(&bmp_icon_info, _("Cancel"), _("Confirm"),
                      _("Verified binary message"), str[0], str[1], str[2],
                      str[3], NULL, NULL);
  } else {
    str = split_message(msg, len, 20);
    layoutDialogSwipe(&bmp_icon_info, _("Cancel"), _("Confirm"),
                      _("Verified message"), str[0], str[1], str[2], str[3],
                      NULL, NULL);
  }
}

void layoutVerifyAddress(const CoinInfo *coin, const char *address) {
  render_address_dialog(coin, address, _("Confirm address?"),
                        _("Message signed by:"), 0);
}

void layoutCipherKeyValue(bool encrypt, const char *key) {
  const char **str = split_message((const uint8_t *)key, strlen(key), 16);
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"),
                    encrypt ? _("Encrypt value of this key?")
                            : _("Decrypt value of this key?"),
                    str[0], str[1], str[2], str[3], NULL, NULL);
}

void layoutEncryptMessage(const uint8_t *msg, uint32_t len, bool signing) {
  const char **str = split_message(msg, len, 16);
  layoutDialogSwipe(
      &bmp_icon_question, _("Cancel"), _("Confirm"),
      signing ? _("Encrypt+Sign message?") : _("Encrypt message?"), str[0],
      str[1], str[2], str[3], NULL, NULL);
}

void layoutDecryptMessage(const uint8_t *msg, uint32_t len,
                          const char *address) {
  const char **str = split_message(msg, len, 16);
  layoutDialogSwipe(
      &bmp_icon_info, NULL, _("OK"),
      address ? _("Decrypted signed message") : _("Decrypted message"), str[0],
      str[1], str[2], str[3], NULL, NULL);
}

void layoutResetWord(const char *word, int pass, int word_pos, bool last) {
  layoutLast = layoutResetWord;
  layoutSwipe();

  const char *btnYes = NULL;
  if (last) {
    if (pass == 1) {
      btnYes = _("Finish");
    } else {
      btnYes = _("Again");
    }
  } else {
    btnYes = _("Next");
  }

  const char *action = NULL;
  if (pass == 1) {
    action = _("Please check the seed");
  } else {
    action = _("Write down the seed");
  }

  char index_str[] = "##th word is:";
  if (word_pos < 10) {
    index_str[0] = ' ';
  } else {
    index_str[0] = '0' + word_pos / 10;
  }
  index_str[1] = '0' + word_pos % 10;
  if (word_pos == 1 || word_pos == 21) {
    index_str[2] = 's';
    index_str[3] = 't';
  } else if (word_pos == 2 || word_pos == 22) {
    index_str[2] = 'n';
    index_str[3] = 'd';
  } else if (word_pos == 3 || word_pos == 23) {
    index_str[2] = 'r';
    index_str[3] = 'd';
  }

  int left = 0;
  oledClear();
  oledDrawBitmap(0, 0, &bmp_icon_info);
  left = bmp_icon_info.width + 4;

  oledDrawString(left, 0 * 9, action, FONT_STANDARD);
  oledDrawString(left, 2 * 9, word_pos < 10 ? index_str + 1 : index_str,
                 FONT_STANDARD);
  oledDrawStringCenter(OLED_WIDTH / 2, 4 * 9 - 3, word,
                       FONT_FIXED | FONT_DOUBLE);
  // 30 is the maximum pixels used for a pixel row in the BIP39 word "abstract"
  oledSCA(4 * 9 - 3 - 2, 4 * 9 - 3 + 15 + 2, 30);
  oledInvert(0, 4 * 9 - 3 - 2, OLED_WIDTH - 1, 4 * 9 - 3 + 15 + 2);
  layoutButtonYes(btnYes, &bmp_btn_confirm);
  oledRefresh();
}

#define QR_MAX_VERSION 9

void layoutAddress(const char *address, const char *desc, bool qrcode,
                   bool ignorecase, const uint32_t *address_n,
                   size_t address_n_count, bool address_is_account) {
  if (layoutLast != layoutAddress) {
    layoutSwipe();
  } else {
    oledClear();
  }
  layoutLast = layoutAddress;

  uint32_t addrlen = strlen(address);
  if (qrcode) {
    char address_upcase[addrlen + 1];
    memset(address_upcase, 0, sizeof(address_upcase));
    if (ignorecase) {
      for (uint32_t i = 0; i < addrlen + 1; i++) {
        address_upcase[i] = address[i] >= 'a' && address[i] <= 'z'
                                ? address[i] + 'A' - 'a'
                                : address[i];
      }
    }
    uint8_t codedata[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)] = {0};
    uint8_t tempdata[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)] = {0};

    int side = 0;
    if (qrcodegen_encodeText(ignorecase ? address_upcase : address, tempdata,
                             codedata, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                             QR_MAX_VERSION, qrcodegen_Mask_AUTO, true)) {
      side = qrcodegen_getSize(codedata);
    }

    oledInvert(0, 0, 63, 63);
    if (side > 0 && side <= 29) {
      int offset = 32 - side;
      for (int i = 0; i < side; i++) {
        for (int j = 0; j < side; j++) {
          if (qrcodegen_getModule(codedata, i, j)) {
            oledBox(offset + i * 2, offset + j * 2, offset + 1 + i * 2,
                    offset + 1 + j * 2, false);
          }
        }
      }
    } else if (side > 0 && side <= 60) {
      int offset = 32 - (side / 2);
      for (int i = 0; i < side; i++) {
        for (int j = 0; j < side; j++) {
          if (qrcodegen_getModule(codedata, i, j)) {
            oledClearPixel(offset + i, offset + j);
          }
        }
      }
    }
  } else {
    if (desc) {
      oledDrawString(0, 0 * 9, desc, FONT_STANDARD);
    }
    if (addrlen > 10) {  // don't split short addresses
      uint32_t rowlen =
          (addrlen - 1) / (addrlen <= 42 ? 2 : addrlen <= 63 ? 3 : 4) + 1;
      const char **str =
          split_message((const uint8_t *)address, addrlen, rowlen);
      for (int i = 0; i < 4; i++) {
        oledDrawString(0, (i + 1) * 9 + 4, str[i], FONT_FIXED);
      }
    } else {
      oledDrawString(0, (0 + 1) * 9 + 4, address, FONT_FIXED);
    }
    oledDrawString(
        0, 42, address_n_str(address_n, address_n_count, address_is_account),
        FONT_STANDARD);
  }

  if (!qrcode) {
    layoutButtonNo(_("QR Code"), NULL);
  }

  layoutButtonYes(_("Continue"), &bmp_btn_confirm);
  oledRefresh();
}

void layoutPublicKey(const uint8_t *pubkey) {
  char desc[16] = {0};
  strlcpy(desc, "Public Key: 00", sizeof(desc));
  if (pubkey[0] == 1) {
    /* ed25519 public key */
    // pass - leave 00
  } else {
    data2hex(pubkey, 1, desc + 12);
  }
  const char **str = split_message_hex(pubkey + 1, 32 * 2);
  layoutDialogSwipe(&bmp_icon_question, NULL, _("Continue"), NULL, desc, str[0],
                    str[1], str[2], str[3], NULL);
}

void layoutSignIdentity(const IdentityType *identity, const char *challenge) {
  char row_proto[8 + 11 + 1] = {0};
  char row_hostport[64 + 6 + 1] = {0};
  char row_user[64 + 8 + 1] = {0};

  bool is_gpg = (strcmp(identity->proto, "gpg") == 0);

  if (identity->has_proto && identity->proto[0]) {
    if (strcmp(identity->proto, "https") == 0) {
      strlcpy(row_proto, _("Web sign in to:"), sizeof(row_proto));
    } else if (is_gpg) {
      strlcpy(row_proto, _("GPG sign for:"), sizeof(row_proto));
    } else {
      strlcpy(row_proto, identity->proto, sizeof(row_proto));
      char *p = row_proto;
      while (*p) {
        *p = toupper((int)*p);
        p++;
      }
      strlcat(row_proto, _(" login to:"), sizeof(row_proto));
    }
  } else {
    strlcpy(row_proto, _("Login to:"), sizeof(row_proto));
  }

  if (identity->has_host && identity->host[0]) {
    strlcpy(row_hostport, identity->host, sizeof(row_hostport));
    if (identity->has_port && identity->port[0]) {
      strlcat(row_hostport, ":", sizeof(row_hostport));
      strlcat(row_hostport, identity->port, sizeof(row_hostport));
    }
  } else {
    row_hostport[0] = 0;
  }

  if (identity->has_user && identity->user[0]) {
    strlcpy(row_user, _("user: "), sizeof(row_user));
    strlcat(row_user, identity->user, sizeof(row_user));
  } else {
    row_user[0] = 0;
  }

  if (is_gpg) {
    // Split "First Last <first@last.com>" into 2 lines:
    // "First Last"
    // "first@last.com"
    char *email_start = strchr(row_hostport, '<');
    if (email_start) {
      strlcpy(row_user, email_start + 1, sizeof(row_user));
      *email_start = 0;
      char *email_end = strchr(row_user, '>');
      if (email_end) {
        *email_end = 0;
      }
    }
  }

  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"),
                    _("Do you want to sign in?"),
                    row_proto[0] ? row_proto : NULL,
                    row_hostport[0] ? row_hostport : NULL,
                    row_user[0] ? row_user : NULL, challenge, NULL, NULL);
}

void layoutDecryptIdentity(const IdentityType *identity) {
  char row_proto[8 + 11 + 1] = {0};
  char row_hostport[64 + 6 + 1] = {0};
  char row_user[64 + 8 + 1] = {0};

  if (identity->has_proto && identity->proto[0]) {
    strlcpy(row_proto, identity->proto, sizeof(row_proto));
    char *p = row_proto;
    while (*p) {
      *p = toupper((int)*p);
      p++;
    }
    strlcat(row_proto, _(" decrypt for:"), sizeof(row_proto));
  } else {
    strlcpy(row_proto, _("Decrypt for:"), sizeof(row_proto));
  }

  if (identity->has_host && identity->host[0]) {
    strlcpy(row_hostport, identity->host, sizeof(row_hostport));
    if (identity->has_port && identity->port[0]) {
      strlcat(row_hostport, ":", sizeof(row_hostport));
      strlcat(row_hostport, identity->port, sizeof(row_hostport));
    }
  } else {
    row_hostport[0] = 0;
  }

  if (identity->has_user && identity->user[0]) {
    strlcpy(row_user, _("user: "), sizeof(row_user));
    strlcat(row_user, identity->user, sizeof(row_user));
  } else {
    row_user[0] = 0;
  }

  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"),
                    _("Do you want to decrypt?"),
                    row_proto[0] ? row_proto : NULL,
                    row_hostport[0] ? row_hostport : NULL,
                    row_user[0] ? row_user : NULL, NULL, NULL, NULL);
}

#if U2F_ENABLED

void layoutU2FDialog(const char *verb, const char *appname) {
  layoutDialog(&bmp_webauthn, NULL, verb, NULL, verb, _("U2F security key?"),
               NULL, appname, NULL, NULL);
}

#endif

#if !BITCOIN_ONLY

void layoutNEMDialog(const BITMAP *icon, const char *btnNo, const char *btnYes,
                     const char *desc, const char *line1, const char *address) {
  static char first_third[NEM_ADDRESS_SIZE / 3 + 1];
  strlcpy(first_third, address, sizeof(first_third));

  static char second_third[NEM_ADDRESS_SIZE / 3 + 1];
  strlcpy(second_third, &address[NEM_ADDRESS_SIZE / 3], sizeof(second_third));

  const char *third_third = &address[NEM_ADDRESS_SIZE * 2 / 3];

  layoutDialogSwipe(icon, btnNo, btnYes, desc, line1, first_third, second_third,
                    third_third, NULL, NULL);
}

void layoutNEMTransferXEM(const char *desc, uint64_t quantity,
                          const bignum256 *multiplier, uint64_t fee) {
  char str_out[32] = {0}, str_fee[32] = {0};

  nem_mosaicFormatAmount(NEM_MOSAIC_DEFINITION_XEM, quantity, multiplier,
                         str_out, sizeof(str_out));
  nem_mosaicFormatAmount(NEM_MOSAIC_DEFINITION_XEM, fee, NULL, str_fee,
                         sizeof(str_fee));

  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Next"), desc,
                    _("Confirm transfer of"), str_out, _("and network fee of"),
                    str_fee, NULL, NULL);
}

void layoutNEMNetworkFee(const char *desc, bool confirm, const char *fee1_desc,
                         uint64_t fee1, const char *fee2_desc, uint64_t fee2) {
  char str_fee1[32] = {0}, str_fee2[32] = {0};

  nem_mosaicFormatAmount(NEM_MOSAIC_DEFINITION_XEM, fee1, NULL, str_fee1,
                         sizeof(str_fee1));

  if (fee2_desc) {
    nem_mosaicFormatAmount(NEM_MOSAIC_DEFINITION_XEM, fee2, NULL, str_fee2,
                           sizeof(str_fee2));
  }

  layoutDialogSwipe(
      &bmp_icon_question, _("Cancel"), confirm ? _("Confirm") : _("Next"), desc,
      fee1_desc, str_fee1, fee2_desc, fee2_desc ? str_fee2 : NULL, NULL, NULL);
}

void layoutNEMTransferMosaic(const NEMMosaicDefinition *definition,
                             uint64_t quantity, const bignum256 *multiplier,
                             uint8_t network) {
  char str_out[32] = {0}, str_levy[32] = {0};

  nem_mosaicFormatAmount(definition, quantity, multiplier, str_out,
                         sizeof(str_out));

  if (definition->has_levy) {
    nem_mosaicFormatLevy(definition, quantity, multiplier, network, str_levy,
                         sizeof(str_levy));
  }

  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Next"),
                    definition->has_name ? definition->name : _("Mosaic"),
                    _("Confirm transfer of"), str_out,
                    definition->has_levy ? _("and levy of") : NULL,
                    definition->has_levy ? str_levy : NULL, NULL, NULL);
}

void layoutNEMTransferUnknownMosaic(const char *namespace, const char *mosaic,
                                    uint64_t quantity,
                                    const bignum256 *multiplier) {
  char mosaic_name[32] = {0};
  nem_mosaicFormatName(namespace, mosaic, mosaic_name, sizeof(mosaic_name));

  char str_out[32] = {0};
  nem_mosaicFormatAmount(NULL, quantity, multiplier, str_out, sizeof(str_out));

  char *decimal = strchr(str_out, '.');
  if (decimal != NULL) {
    *decimal = '\0';
  }

  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("I take the risk"),
                    _("Unknown Mosaic"), _("Confirm transfer of"), str_out,
                    _("raw units of"), mosaic_name, NULL, NULL);
}

void layoutNEMTransferPayload(const uint8_t *payload, size_t length,
                              bool encrypted) {
  if (length >= 1 && payload[0] == 0xFE) {
    char encoded[(length - 1) * 2 + 1];
    memset(encoded, 0, sizeof(encoded));

    data2hex(&payload[1], length - 1, encoded);

    const char **str =
        split_message((uint8_t *)encoded, sizeof(encoded) - 1, 16);
    layoutDialogSwipe(
        &bmp_icon_question, _("Cancel"), _("Next"),
        encrypted ? _("Encrypted hex data") : _("Unencrypted hex data"), str[0],
        str[1], str[2], str[3], NULL, NULL);
  } else {
    const char **str = split_message(payload, length, 16);
    layoutDialogSwipe(
        &bmp_icon_question, _("Cancel"), _("Next"),
        encrypted ? _("Encrypted message") : _("Unencrypted message"), str[0],
        str[1], str[2], str[3], NULL, NULL);
  }
}

void layoutNEMMosaicDescription(const char *description) {
  const char **str =
      split_message((uint8_t *)description, strlen(description), 16);
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Next"),
                    _("Mosaic Description"), str[0], str[1], str[2], str[3],
                    NULL, NULL);
}

void layoutNEMLevy(const NEMMosaicDefinition *definition, uint8_t network) {
  const NEMMosaicDefinition *mosaic = NULL;
  if (nem_mosaicMatches(definition, definition->levy_namespace,
                        definition->levy_mosaic, network)) {
    mosaic = definition;
  } else {
    mosaic = nem_mosaicByName(definition->levy_namespace,
                              definition->levy_mosaic, network);
  }

  char mosaic_name[32] = {0};
  if (mosaic == NULL) {
    nem_mosaicFormatName(definition->levy_namespace, definition->levy_mosaic,
                         mosaic_name, sizeof(mosaic_name));
  }

  char str_out[32] = {0};

  switch (definition->levy) {
    case NEMMosaicLevy_MosaicLevy_Percentile:
      bn_format_uint64(definition->fee, NULL, NULL, 0, 0, false, str_out,
                       sizeof(str_out));

      layoutDialogSwipe(
          &bmp_icon_question, _("Cancel"), _("Next"), _("Percentile Levy"),
          _("Raw levy value is"), str_out, _("in"),
          mosaic ? (mosaic == definition ? _("the same mosaic") : mosaic->name)
                 : mosaic_name,
          NULL, NULL);
      break;

    case NEMMosaicLevy_MosaicLevy_Absolute:
    default:
      nem_mosaicFormatAmount(mosaic, definition->fee, NULL, str_out,
                             sizeof(str_out));
      layoutDialogSwipe(
          &bmp_icon_question, _("Cancel"), _("Next"), _("Absolute Levy"),
          _("Levy is"), str_out,
          mosaic ? (mosaic == definition ? _("in the same mosaic") : NULL)
                 : _("in raw units of"),
          mosaic ? NULL : mosaic_name, NULL, NULL);
      break;
  }
}

#endif

static inline bool is_slip18(const uint32_t *address_n,
                             size_t address_n_count) {
  return address_n_count == 2 && address_n[0] == (0x80000000 + 10018) &&
         (address_n[1] & 0x80000000) && (address_n[1] & 0x7FFFFFFF) <= 9;
}

void layoutCosiCommitSign(const uint32_t *address_n, size_t address_n_count,
                          const uint8_t *data, uint32_t len, bool final_sign) {
  char *desc = final_sign ? _("CoSi sign message?") : _("CoSi commit message?");
  char desc_buf[32] = {0};
  if (is_slip18(address_n, address_n_count)) {
    if (final_sign) {
      strlcpy(desc_buf, _("CoSi sign index #?"), sizeof(desc_buf));
      desc_buf[16] = '0' + (address_n[1] & 0x7FFFFFFF);
    } else {
      strlcpy(desc_buf, _("CoSi commit index #?"), sizeof(desc_buf));
      desc_buf[18] = '0' + (address_n[1] & 0x7FFFFFFF);
    }
    desc = desc_buf;
  }
  char str[4][17] = {0};
  if (len == 32) {
    data2hex(data, 8, str[0]);
    data2hex(data + 8, 8, str[1]);
    data2hex(data + 16, 8, str[2]);
    data2hex(data + 24, 8, str[3]);
  } else {
    strlcpy(str[0], "Data", sizeof(str[0]));
    strlcpy(str[1], "of", sizeof(str[1]));
    strlcpy(str[2], "unsupported", sizeof(str[2]));
    strlcpy(str[3], "length", sizeof(str[3]));
  }
  layoutDialogSwipe(&bmp_icon_question, _("Cancel"), _("Confirm"), desc, str[0],
                    str[1], str[2], str[3], NULL, NULL);
}

void Disp_Page(const BITMAP *icon, const char *btnNo, const char *btnYes,
               const char *desc, uint8_t *pucInfoBuf, uint16_t usLen) {
  char j, line1[33], line2[33], line3[33], line4[33], line5[33], line6[33];
  j = 0;
  if (usLen > 32) {
    usLen = 32;
  }
  memzero(line1, sizeof(line1));
  memzero(line2, sizeof(line2));
  memzero(line3, sizeof(line3));
  memzero(line4, sizeof(line4));
  memzero(line5, sizeof(line5));
  memzero(line6, sizeof(line6));
  memcpy(line1, (char *)(pucInfoBuf + j * usLen), usLen);
  j++;
  memcpy(line2, (char *)(pucInfoBuf + j * usLen), usLen);
  j++;
  memcpy(line3, (char *)(pucInfoBuf + j * usLen), usLen);
  j++;
  memcpy(line4, (char *)(pucInfoBuf + j * usLen), usLen);
  j++;
  memcpy(line5, (char *)(pucInfoBuf + j * usLen), usLen);
  j++;
  memcpy(line6, (char *)(pucInfoBuf + j * usLen), usLen);

  layoutDialog(icon, btnNo, btnYes, desc, line1, line2, line3, line4, line5,
               line6);
}

void vDISP_TurnPageUP(void) {
  if (s_usCurrentCount == 0) {
    return;
  }
  s_usCurrentCount = s_usCurrentCount - DISP_PAGESIZE;
  Disp_Page(&bmp_icon_question, _("Up"), _("Down"), NULL,
            Disp_buffer + s_usCurrentCount, 16);
}

void vDISP_TurnPageDOWN(void) {
  if ((s_usCurrentCount + DISP_PAGESIZE) >= s_uiShowLength) {
    return;
  }
  s_usCurrentCount += DISP_PAGESIZE;
  Disp_Page(&bmp_icon_question, _("Up"), _("Down"), NULL,
            Disp_buffer + s_usCurrentCount, 16);
}
void layoutDeviceInfo(uint8_t ucPage) {
  uint8_t ucBuf[66];
  uint8_t line[17];

  switch (ucPage) {
    case 0:
      oledClear();
      oledDrawString(0, 0, (char *)USB_LABLE, FONT_STANDARD);
      oledDrawStringCenter(64, 8, (char *)DEFAULTLABE, FONT_STANDARD);

      oledDrawString(0, 24, (char *)USB_SN, FONT_STANDARD);
      oledDrawStringCenter(64, 32, (char *)DEFAULSN, FONT_STANDARD);

      oledDrawString(0, 48, (char *)USB_VER, FONT_STANDARD);
      memzero(ucBuf, sizeof(ucBuf));
      ucBuf[0] = VERSION_MAJOR + '0';
      ucBuf[1] = '.';
      ucBuf[2] = VERSION_MINOR + '0';
      ucBuf[3] = '.';
      ucBuf[4] = VERSION_PATCH + '0';
      oledDrawString(64, 56, (char *)ucBuf, FONT_STANDARD);
      break;
    case 1:
      oledClear();
      oledDrawString(0, 0, (char *)APP_FINGERPRINT, FONT_STANDARD);
      memzero(g_usb_info.ucfingerprint, sizeof(g_usb_info.ucfingerprint));
      sha256_Raw(FLASH_PTR(FLASH_APP_START), (64 - 1) * 1024,
                 g_usb_info.ucfingerprint);
      data2hex(g_usb_info.ucfingerprint, 32, (char *)ucBuf);
      memzero(line, sizeof(line));
      memcpy(line, ucBuf, 0x10);
      oledDrawStringCenter(64, 16, (char *)line, FONT_STANDARD);
      memzero(line, sizeof(line));
      memcpy(line, ucBuf + 0x10, 0x10);
      oledDrawStringCenter(64, 24, (char *)line, FONT_STANDARD);
      memzero(line, sizeof(line));
      memcpy(line, ucBuf + 0x20, 0x10);
      oledDrawStringCenter(64, 32, (char *)line, FONT_STANDARD);
      memzero(line, sizeof(line));
      memcpy(line, ucBuf + 0x30, 0x10);
      oledDrawStringCenter(64, 40, (char *)line, FONT_STANDARD);
      break;
    case 2:
      oledClear();
      oledDrawString(0, 0, (char *)BLE_NAME, FONT_STANDARD);
      oledDrawStringCenter(64, 8, (char *)g_ble_info.ucBle_Name, FONT_STANDARD);
      oledDrawString(0, 16, (char *)BLE_MAC, FONT_STANDARD);
      memzero(ucBuf, sizeof(ucBuf));
      data2hex(g_ble_info.ucBle_Mac, BLE_MAC_LEN, (char *)ucBuf);
      oledDrawStringCenter(64, 24, (char *)ucBuf, FONT_STANDARD);

      oledDrawString(0, 32, (char *)BLE_VER, FONT_STANDARD);
      memzero(ucBuf, sizeof(ucBuf));
      ucBuf[0] = ((g_ble_info.ucBle_Version[0] & 0xF0) >> 4) + '0';
      ucBuf[1] = '.';
      ucBuf[2] = (g_ble_info.ucBle_Version[0] & 0x0F) + '0';
      ucBuf[3] = '.';
      ucBuf[4] = ((g_ble_info.ucBle_Version[1] & 0xF0) >> 4) + '0';
      ucBuf[5] = (g_ble_info.ucBle_Version[1] & 0x0F) + '0';

      oledDrawString(64, 48, (char *)ucBuf, FONT_STANDARD);
      break;
  }
  oledRefresh();
  layoutLast = layoutDeviceInfo;
}

void layoutHomeInfo(void) {
  static uint8_t info_page = 0;
  buttonUpdate();

  if (layoutLast == layoutHome) {
    if (button.UpUp || button.DownUp) {
      layoutDeviceInfo(info_page);
    }
  } else if (layoutLast == layoutDeviceInfo) {
    if (button.UpUp) {
      if (info_page)
        info_page--;
      else
        info_page = 2;
      layoutDeviceInfo(info_page);
    } else if (button.DownUp) {
      if (info_page < 2)
        info_page++;
      else
        info_page = 0;
      layoutDeviceInfo(info_page);
    } else if (button.NoUp) {
      layoutHome();
    }
  }
  // if homescreen is shown for too long
  if (layoutLast == layoutHome) {
    if ((timer_ms() - system_millis_lock_start) >=
        config_getAutoLockDelayMs()) {
      // lock the screen
      session_clear(true);
      layoutScreensaver();
    }
  }
  // wake from screensaver on any button
  if (layoutLast == layoutScreensaver && (button.NoUp || button.YesUp)) {
    layoutHome();
    return;
  }
}

/*
 * display prompt info
 */
void vDisp_PromptInfo(uint8_t ucIndex, bool ucMode) {
  if (ucMode) {
    oledClear();
  }
  switch (ucIndex) {
    case DISP_NOT_ACTIVE:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_unactive);
      } else {
        oledDrawStringCenter(60, 48, "Not Activated", FONT_STANDARD);
      }
      break;
    case DISP_TOUCHPH:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_touch_phone);
      } else {
        oledDrawStringCenter(60, 48, "It needs to", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "touch the phone", FONT_STANDARD);
      }
      break;
    case DISP_NFC_LINK:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_nfc_link);
      } else {
        oledDrawStringCenter(60, 48, "Connect by NFC", FONT_STANDARD);
      }
      break;
    case DISP_USB_LINK:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_usb_link);
      } else {
        oledDrawStringCenter(60, 48, "Connect by USB", FONT_STANDARD);
      }
      break;
    case DISP_COMPUTER_LINK:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_computerlink);
      } else {
        oledDrawStringCenter(60, 48, "Connect to a computer", FONT_STANDARD);
      }
      break;
    case DISP_INPUTPIN:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 0, &bmp_cn_input_pin);
      } else {
        oledDrawStringCenter(60, 40, "Enter PIN code", FONT_STANDARD);
        oledDrawStringCenter(60, 48, "according to prompts", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "on the right screen", FONT_STANDARD);
      }
      break;
    case DISP_BUTTON_OK_RO_NO:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_button_yes_no);
      } else {
        oledDrawStringCenter(60, 48, "Press OK to confirm, ", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "Press < to Cancel", FONT_STANDARD);
      }
      break;
    case DISP_GEN_PRI_KEY:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_prikey_gen);
      } else {
        oledDrawStringCenter(60, 48, "Generating private key...",
                             FONT_STANDARD);
      }
      break;
    case DISP_ACTIVE_SUCCESS:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_active_success);
      } else {
        oledDrawStringCenter(60, 48, "Activated", FONT_STANDARD);
      }
      break;
    case DISP_BOTTON_UP_OR_DOWN:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_updown_view);
      } else {
        oledDrawStringCenter(60, 30, "Turn left or right to view",
                             FONT_STANDARD);
      }
      break;
    case DISP_SN:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_sn);
      } else {
        oledDrawStringCenter(60, 48, "Serial NO.", FONT_STANDARD);
      }
      break;
    case DISP_VERSION:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_version);
      } else {
        oledDrawStringCenter(60, 48, "Firmware version", FONT_STANDARD);
      }
      break;
    case DISP_CONFIRM_PUB_KEY:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_confirm_pubkey);
      } else {
        oledDrawStringCenter(60, 48, "Confirm public key", FONT_STANDARD);
      }
      break;
    case DISP_BOTTON_OK_SIGN:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 48, &bmp_cn_sign_ok);
      } else {
        oledDrawStringCenter(60, 48, "Press OK to sign", FONT_STANDARD);
      }
      break;
    case DISP_SIGN_SUCCESS:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_sign_success_phone);
      } else {
        oledDrawStringCenter(60, 32, "Signed! Touch it to", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "the phone closely", FONT_STANDARD);
      }
      break;
    case DISP_SIGN_PRESS_OK_HOME:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_sign_success_gohome);
      } else {
        oledDrawStringCenter(60, 32, "Signed! Press OK to", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "return to homepage", FONT_STANDARD);
      }
      break;
    case DISP_SIGN_SUCCESS_VIEW:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_sign_ok_view);
      } else {
        oledDrawStringCenter(60, 40, "Signed!", FONT_STANDARD);
        oledDrawStringCenter(60, 48, "Please view transaction", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "on your phone", FONT_STANDARD);
      }
      break;
    case DISP_UPDATGE_APP_GOING:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_updating_notpower_off);
      } else {
        oledDrawStringCenter(60, 48, "Upgrading,", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "do not turn off", FONT_STANDARD);
      }
      break;
    case DISP_UPDATGE_SUCCESS:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_update_sucess);
      } else {
        oledDrawStringCenter(60, 40, "Firmware upgraded,", FONT_STANDARD);
        oledDrawStringCenter(60, 48, "press OK to ", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "return to homepage", FONT_STANDARD);
      }
      break;
    case DISP_PRESSKEY_POWEROFF:
      oledClear();
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 0, &bmp_cn_poweroff);
      } else {
        oledDrawStringCenter(60, 30, "Power Off", FONT_STANDARD);
      }
      oledRefresh();
      delay(2000);
      oledClear();
      oledRefresh();
      return;
    case DISP_BLE_NAME:
      oledDrawStringCenter(60, 56, (const char *)g_ble_info.ucBle_Name,
                           FONT_STANDARD);
      break;
    case DISP_EXPORT_PRIVATE_KEY:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_export_encrypted_prikey);
      } else {
        oledDrawStringCenter(60, 48, "[Encrypted]", FONT_STANDARD);
        oledDrawStringCenter(60, 56, "Exporting private key…", FONT_STANDARD);
      }
      break;
    case DISP_IMPORT_PRIVATE_KEY:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_import_prikey);
      } else {
        oledDrawStringCenter(60, 56, "Importing private key", FONT_STANDARD);
      }
      break;
    case DISP_UPDATE_SETTINGS:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_update_settings);
      } else {
        oledDrawStringCenter(60, 56, "Settings updated", FONT_STANDARD);
      }
      break;
    case DISP_BIXIN_KEY_INITIALIZED:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 32, &bmp_cn_BixinKEY_initialized);
      } else {
        oledDrawStringCenter(60, 56, "BixinKEY initialized", FONT_STANDARD);
      }
      break;
    case DISP_CONFIRM_PIN:
      if (g_ucLanguageFlag) {
        oledDrawBitmap(0, 16, &bmp_cn_confirm_pin);
      }
      break;
    default:
      break;
  }
  if (ucMode) {
    oledRefresh();
  }
  g_ucPromptIndex = 0;
}
