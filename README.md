# E-Ink PDA Calendar Module

This project is part of the **EinkPDA project** by **ashft8**, and provides a complete calendar system for an ESP32-S3-based PDA device equipped with both an E-Ink display and an OLED screen.

It is written in **C++ for the Arduino IDE**, strictly following best practices for embedded systems (based on the NASA JPL coding standards). The system is fully self-contained and handles all calendar logic, rendering, data storage, and user input.

---

## Features

- 📆 **Month view**, **week view**, and **day view**
- 🕒 **Time block scrolling** (Night, Morning, Afternoon, Evening)
- 💾 Save and load events and tasks from SD card
- 📲 Input new events via **OLED keyboard entry**, like:  
  `Tue 1500 Lunch with Bob` or `Mon 7:30am Gym`
- 🖼️ Smart refresh system: every 5th refresh is a **full refresh** to reduce ghosting
- 📅 Tasks from `TASKS.ino` also appear in the calendar
- ⏰ Automatic selection of correct time block on load
- 🔁 Double spacebar jump to **today's view**
- 🧠 Written for clarity, modularity, and real-world robustness

---

## Project Structure

### `runCalendar()`
Main loop that runs the calendar and reacts to input.

### Display Modes

- **Month View**: Grid view of days in the month. Days with entries or tasks are marked.
- **Week View**: Scrollable 6-hour time blocks across all 7 days. Entries and tasks shown.
- **Day View**: Detailed breakdown of a single day, with entries/tasks shown by time slot.

### Input System

- `FN1` → Switch to week view
- `FN2` → Switch to month view
- `1`–`5` → Jump to week (in month view)
- `1`–`7` → Jump to day (in week view)
- `ENTER` → In week view: create entry via OLED input
- `Space` → Context-sensitive: jump to today or back

---

## File Structure

- `CALENDAR.ino` – Main calendar logic
- `/calendar/YYYY_MM.txt` – Event storage format:
  ```
  2025-04-15 15:00 Lunch with Bob
  ```
- `/tasks.txt` – Task storage, shared with `TASKS.ino`:
  ```
  Feed chickens|2025-04-18|...
  ```

---

## Dependencies

- Compatible with **ESP32-S3** boards
- Uses the following custom modules:
  - `OLEDFunc.h` – OLED input & display
  - `einkFunc.h` – E-Ink rendering (with smart refresh)

---

## Credits

This module was created as part of the **EinkPDA open hardware project by ashft8**, with extensions and calendar logic by contributors.  
Designed to be modular, robust, and suitable for daily use on a minimalist PDA.

---

## License

MIT or equivalent – free to use and modify for open-source and personal projects.
