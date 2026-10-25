<!-- Document made so that anyone who contributes using AI agents don't mess up -->

# AI Agent Guidelines for Wii Motion Synth

This repository is public. When assisting with this codebase, all AI agents and coding assistants must adhere to the following rules:

1. **Professional Code Comments:** Do not include personalized chat references, conversational filler, or references to "we" or "us" in code comments. Keep all documentation and inline comments strictly technical, professional, and in English.
2. **No "Vibecoded" Clichés:** When generating or editing documentation (like `README.md`), avoid overconfident claims, excessive emojis (✨), or unnecessary padding. The tone should be concise, developer-focused, and direct.
3. **Hardware Context:** This is a Nintendo Wii homebrew application written in C using `libogc` and `devkitPPC`. Keep in mind the hardware limitations of the PowerPC Gekko processor (e.g., avoid FPU math inside IRQ callbacks, ensure 32-byte alignment for DMA buffers).
4. **Attribution:** If suggesting code snippets derived from other open-source projects or homebrew libraries, clearly indicate the source and its license.