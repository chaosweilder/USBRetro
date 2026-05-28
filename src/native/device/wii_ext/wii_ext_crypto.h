// wii_ext_crypto.h - Wii Extension Controller Encryption
//
// Implements the encryption cipher used by Wii extension controllers
// (Classic Controller, Nunchuck, etc.) when communicating with a Wiimote
// connected to a real Wii console. The Wii sends an encryption key to
// register 0xF0/0x40, and all subsequent report data must be encrypted.
//
// Algorithm and S-box data derived from Hector Martin (marcan)'s original
// 2008 reverse engineering of the Wii hardware, published on wiibrew.org
// and the sci.crypt mailing list. The S-box tables are discovered hardware
// constants, not creative works.
//
// References:
//   https://wiibrew.org/wiki/Wiimote/Extension_Controllers
//   https://sci.crypt.narkive.com/g1zYewWd/reverse-engineering-the-wiimote-s-encryption
//
// SPDX-License-Identifier: Apache-2.0

#ifndef WII_EXT_CRYPTO_H
#define WII_EXT_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

// Initialize encryption tables from the 16-byte key written to reg[0x40-0x4F].
// The key is split into key[0-5] (bytes 0x40-0x45) and rand[0-9] (bytes 0x46-0x4F).
void wii_ext_crypto_init(const uint8_t key_data[16]);

// Encrypt data in-place before sending to the Wiimote.
// addr = register address of the first byte (usually 0x00 for reports).
void wii_ext_crypto_encrypt(uint8_t* data, uint8_t addr, uint8_t len);

// Decrypt data in-place after receiving from the Wiimote. The Wiimote
// encrypts writes to extension registers when encryption is active, so
// we must decrypt them before storing or interpreting.
void wii_ext_crypto_decrypt(uint8_t* data, uint8_t addr, uint8_t len);

// Whether encryption is currently active (0xAA written to 0xF0).
extern bool wii_ext_crypto_enabled;

// One-shot self-test: runs the real init + encrypt code paths against a
// known key/plaintext/ciphertext triple (captured from an actual Wii and
// verified against Dolphin's reference implementation). Prints PASS/FAIL
// for each step. Does not leave module state disturbed.
void wii_ext_crypto_self_test(void);

#endif // WII_EXT_CRYPTO_H
