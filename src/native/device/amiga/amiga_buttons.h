// amiga_buttons.h - Amiga/CD32 button bit positions
//
// CD32 serial protocol bit order (active LOW, 0=pressed):
// Bit 0: Blue    (maps to JP_BUTTON_B1 / South / Cross)
// Bit 1: Red     (maps to JP_BUTTON_B2 / East  / Circle)
// Bit 2: Yellow  (maps to JP_BUTTON_B3 / West  / Square)
// Bit 3: Green   (maps to JP_BUTTON_B4 / North / Triangle)
// Bit 4: Right Front / FF  (maps to JP_BUTTON_R1)
// Bit 5: Left Front  / Rew (maps to JP_BUTTON_L1)
// Bit 6: Pause        (maps to JP_BUTTON_S2 / Start)
// Bit 7: 1 (signature high bit - always 1)
// Bit 8: 0 (signature low bit  - always 0)

#ifndef AMIGA_BUTTONS_H
#define AMIGA_BUTTONS_H

// CD32 serial data bit positions
#define CD32_BIT_BLUE        0   // Blue button   (Fire 2 / B1)
#define CD32_BIT_RED         1   // Red button    (Fire 1 / B2)
#define CD32_BIT_YELLOW      2   // Yellow button (B3)
#define CD32_BIT_GREEN       3   // Green button  (B4)
#define CD32_BIT_RFRONT      4   // Right Front   (R1 / FF)
#define CD32_BIT_LFRONT      5   // Left Front    (L1 / Rew)
#define CD32_BIT_PAUSE       6   // Pause         (Start)
#define CD32_BIT_SIG_HIGH    7   // Signature bit 7 = 1
#define CD32_BIT_SIG_LOW     8   // Signature bit 8 = 0

// CD32 idle word - all buttons released, signature bits set
// Bits are active LOW so 1 = released
// Bit 7 = 1, Bit 8 = 0 is the CD32 signature
#define CD32_IDLE_WORD  0b111111111  // all released (bits 0-6 = 1) + sig bits (7=1, 8=0 -> actually 9 bits total)
// Note: bit 8 is 0 in idle state (part of signature), so idle = 0b011111111 = 0x7F
#define CD32_IDLE       0x7F

#endif // AMIGA_BUTTONS_H
