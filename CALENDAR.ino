#include <Arduino.h>
#include "OLEDFunc.h"
#include "einkFunc.h"
#include "FS.h"
#include "SD.h"

// Current view (0 = month view, 1 = week view, 2 = day view)
uint8_t calendarMode = 0;

// Scroll state for week time blocks: 0 = Night, 1 = Morning, 2 = Afternoon, 3 = Evening
uint8_t weekTimeBlock = 0;

// Current year, month, and week
int currentYear = 2025;
int currentMonth = 1;
int currentWeek = 1;

// Refresh counter for partial vs full refresh
int einkRefreshCount = 0;

// Preview month and year (for scrolling function)
int previewMonth = currentMonth;
int previewYear = currentYear;

// Helper structure for appointments
struct CalendarEntry {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  String title;
};

// ========= Structure for simple task display =========
struct TaskEntry {
  String name;
  int year;
  int month;
  int day;
};

// =================== Main function ====================
void runCalendar() {
  einkClearSmart();
  calendarMode = 0; // Start month view
  drawMonth(currentYear, currentMonth);

  while (true) {
    handleCalendarInput();
    delay(100);
  }
}

// ========== Display month grid ==========
void drawMonth(int year, int month) {
  einkClearSmart();
  einkPrint("Calendar " + String(year) + "/" + (month < 10 ? "0" : "") + String(month));
  einkPrint("Mo Di Mi Do Fr Sa So");

  int startDay = weekdayFromDate(year, month, 1); // 0=Mo, 6=So
  int daysInMonth = getDaysInMonth(year, month);
  
  int todayY, todayM, todayD;
  getToday(todayY, todayM, todayD);
  bool showWeekHighlight = (todayY == year && todayM == month);
  int todayWeek = ((todayD - 1) / 7);

  // Load calendar entries and tasks
  CalendarEntry entries[50];
  int entryCount = loadEntries(year, month, entries, 50);

  TaskEntry tasks[50];
  int taskCount = loadTasks(tasks, 50);

  // Prepare markers for each day
  bool hasEntry[32] = {false};

  for (int i = 0; i < entryCount; i++) {
    if (entries[i].day >= 1 && entries[i].day <= 31) {
      hasEntry[entries[i].day] = true;
    }
  }

  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].year == year && tasks[i].month == month && tasks[i].day >= 1 && tasks[i].day <= 31) {
      hasEntry[tasks[i].day] = true; // merged into single indicator
    }
  }

  int currentDay = 1;
  String line = "";

  for (int week = 0; week < 6; week++) {
    line = "";
    for (int wd = 0; wd < 7; wd++) {
      int cellIndex = week * 7 + wd;
      if (cellIndex >= startDay && currentDay <= daysInMonth) {
        String marker = hasEntry[currentDay] ? "." : " ";
        String dayStr = (currentDay < 10 ? " " : "") + String(currentDay);
        if (showWeekHighlight && week == todayWeek) {
          dayStr = "[" + dayStr + "]";
        }
        line += dayStr + marker + " ";
        currentDay++;
      } else {
        line += "    ";
      }
    }
    einkPrint(line);
  }
}

// Helper function: How many days does a month have
int getDaysInMonth(int year, int month) {
  if (month == 2) {
    // Leap year rule
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

// Helper function: Weekday for a date (Zeller's Congruence, adjusted to 0=Mo)
int weekdayFromDate(int y, int m, int d) {
  if (m < 3) {
    m += 12;
    y -= 1;
  }
  int k = y % 100;
  int j = y / 100;
  int h = (d + 13*(m+1)/5 + k + k/4 + j/4 + 5*j) % 7;
  int wd = ((h + 5) % 7); // 0=Mo, 6=So
  return wd;
}

// ========== Display week view ==========
void drawWeek(int year, int month, int week) {
  einkClearSmart();
  einkPrint("Week " + String(week) + " / " + String(month) + "-" + String(year));
  einkPrint("Enter: Enter text");

  static CalendarEntry weekEntries[50];
  int total = loadEntries(year, month, weekEntries, 50);
  
  TaskEntry weekTasks[50];
  int taskCount = loadTasks(weekTasks, 50);

  displayWeekEntries(weekEntries, total);

  einkPrint("FN2: back to month");
}

// ========== Display week entries ==========
void displayWeekEntries(const CalendarEntry* entries, int count) {
  einkClearSmart();
  einkPrint("Week " + String(currentWeek) + ":");

  TaskEntry tasks[50];
  int taskCount = loadTasks(tasks, 50);

  // Display time range based on scroll block
  int startHour = 6;
  if (weekTimeBlock == 1) startHour = 13;
  else if (weekTimeBlock == 2) startHour = 20;

  int endHour = startHour + 5;
  String rangeLabel = String(startHour) + ":00-" + String(endHour + 1) + ":00";
  einkPrint("Block: " + rangeLabel);

  // Initialize table
  String header = "    |";
  int y, m, d;
  getToday(y, m, d);
  int todayWeek = ((d - 1) / 7) + 1;
  int todayWday = (d - 1) % 7;

  const char* dayNames[7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
  for (int i = 0; i < 7; i++) {
    String name = dayNames[i];
    if (currentYear == y && currentMonth == m && currentWeek == todayWeek && todayWday == i) {
      name = "[" + name + "]";
    }
    while (name.length() < 4) name += " ";
    header += name + "|";
  }
  einkPrint(header);

  // Create grid: 6 hours * 2 = 12 rows (half-hour steps)
  for (int h = startHour; h <= endHour; h++) {
    for (int part = 0; part < 2; part++) {
      String line = (part == 0 ? (h < 10 ? "0" : "") + String(h) + "  |" : "    |");

      for (int d = 0; d < 7; d++) {
        String cell = "     ";

        // Tasks at top (07:00 row only)
        if (h == 6 && part == 0) {
          for (int i = 0; i < taskCount; i++) {
            TaskEntry t = tasks[i];
            int day = t.day;
            if ((day - 1) / 7 + 1 == currentWeek && t.month == currentMonth && t.year == currentYear) {
              int weekday = (t.day - 1) % 7;
              if (weekday == d) cell = t.name.substring(0, 5);
            }
          }
        }

        // Calendar entries
        for (int i = 0; i < count; i++) {
          CalendarEntry e = entries[i];
          if ((e.day - 1) / 7 + 1 == currentWeek && e.month == currentMonth && e.year == currentYear) {
            int weekday = (e.day - 1) % 7;
            if (weekday == d) {
              int rowH = e.hour;
              int rowPart = (e.minute >= 30 ? 1 : 0);
              if (rowH == h && rowPart == part) {
                cell = e.title.substring(0, 5);
              }
            }
          }
        }

        line += cell + "|";
      }
      einkPrint(line);
    }
  }

  showWeekBlockOnOled();
}

// ========== Analyze input & operate calendar ==========
void handleCalendarInput() {
  char key = getKey(); // Must come from keyboard code

  if (key >= '1' && key <= '5' && calendarMode == 0) {
    currentWeek = key - '0';
    calendarMode = 1;
    drawWeek(currentYear, currentMonth, currentWeek);
  }

  if (key == 'A') { // FN1 → week view
    calendarMode = 1;
    drawWeek(currentYear, currentMonth, currentWeek);
  }

  if (key == 'B') { // FN2 → month view
    calendarMode = 0;
    drawMonth(currentYear, currentMonth);
  }

  if (key == '\n' && calendarMode == 1) {
    handleTextInput();
  }

  if (key == '\n' && calendarMode == 0) {
    // ENTER pressed in scroll mode → update calendar
    currentMonth = previewMonth;
    currentYear = previewYear;
    drawMonth(currentYear, currentMonth);
  }

  static unsigned long lastSpace = 0;
  static bool spaceUsed = false;

  if (key == ' ' && calendarMode == 0) {
    unsigned long now = millis();
    if (now - lastSpace < 800 && spaceUsed) {
      jumpToToday(); // doppelte Leertaste = direkt zum heutigen Tag
      spaceUsed = false;
    } else {
      int y, m, d;
      getToday(y, m, d);
      currentYear = y;
      currentMonth = m;
      currentWeek = ((d - 1) / 7) + 1;

      int hour = getCurrentHour();
      weekTimeBlock = (hour < 12) ? 1 : 2;

      calendarMode = 1;
      drawWeek(currentYear, currentMonth, currentWeek);
      spaceUsed = true;
    }
    lastSpace = millis();
  }

  if (key == ' ' && calendarMode == 1) {
    int y, m, d;
    getToday(y, m, d);
    int todayWeek = ((d - 1) / 7) + 1;

    if (currentYear == y && currentMonth == m && currentWeek == todayWeek) {
      jumpToToday(); // aktuelle Woche → Springe in heutigen Tag
    } else {
      calendarMode = 0;
      drawMonth(currentYear, currentMonth); // andere Woche → zurück
    }
  }

  if (key >= '1' && key <= '7' && calendarMode == 1) {
    int weekday = key - '1'; // 0 = Mo
    int dayOfMonth = (currentWeek - 1) * 7 + weekday + 1;
    if (dayOfMonth <= getDaysInMonth(currentYear, currentMonth)) {
      int hour = getCurrentHour();
      if (hour < 6) weekTimeBlock = 0;
      else if (hour < 13) weekTimeBlock = 1;
      else if (hour < 20) weekTimeBlock = 2;
      else weekTimeBlock = 3;

      calendarMode = 2;
      drawDay(currentYear, currentMonth, dayOfMonth);
    }
  }
}

// ========== Input via OLED and save ==========
void handleTextInput() {
  clearOled();
  oledPrint("Input:");
  String input = oledGetLine(); // User enters e.g. "Tue 1500 Lunch with Bob"

  if (input.length() > 0) {
    CalendarEntry entry;
    if (parseEntry(input, entry)) {
      saveEntry(entry);
      einkPrint("Saved: " + entry.title);
    } else {
      einkPrint("Invalid!");
    }
  }
}

// ========== Parser for input ==========
bool parseEntry(String input, CalendarEntry &entry) {
  input.trim();
  int firstSpace = input.indexOf(' ');
  if (firstSpace == -1) return false;

  String dayCode = input.substring(0, firstSpace);
  int secondSpace = input.indexOf(' ', firstSpace + 1);
  if (secondSpace == -1) return false;

  String timeStr = input.substring(firstSpace + 1, secondSpace);
  String title = input.substring(secondSpace + 1);

  int weekday = dayCodeToInt(dayCode);
  if (weekday == -1) return false;

  int hour = -1;
  int minute = -1;

  timeStr.toLowerCase();
  bool isPM = timeStr.indexOf("pm") != -1;
  bool isAM = timeStr.indexOf("am") != -1;

  timeStr.replace("am", "");
  timeStr.replace("pm", "");
  timeStr.trim();

  if ((timeStr.length() == 4 && timeStr.charAt(2) != ':') || timeStr.length() == 4) {
    hour = timeStr.substring(0, 2).toInt();
    minute = timeStr.substring(2, 4).toInt();
  } else if (timeStr.length() >= 4 && timeStr.indexOf(':') != -1) {
    int colon = timeStr.indexOf(':');
    hour = timeStr.substring(0, colon).toInt();
    minute = timeStr.substring(colon + 1).toInt();
  } else {
    return false;
  }

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;

  if (isAM && hour == 12) hour = 0;
  else if (isPM && hour < 12) hour += 12;
  if ((isAM || isPM) && hour > 23) return false;

  entry.year = currentYear;
  entry.month = currentMonth;
  entry.day = (currentWeek - 1) * 7 + weekday + 1;
  entry.hour = hour;
  entry.minute = minute;
  entry.title = title;

  return true;
}

// ========== Extended day parser ==========
int dayCodeToInt(String code) {
  code.toLowerCase();

  if (code == "mo" || code == "mon") return 0;
  if (code == "di" || code == "tu" || code == "tue") return 1;
  if (code == "mi" || code == "we" || code == "wed") return 2;
  if (code == "do" || code == "th" || code == "thu") return 3;
  if (code == "fr" || code == "fri") return 4;
  if (code == "sa" || code == "sat") return 5;
  if (code == "so" || code == "su" || code == "sun") return 6;

  return -1;
}

// ========== Load entries ==========
int loadEntries(int year, int month, CalendarEntry* entries, int maxEntries) {
  String filename = "/calendar/" + String(year) + "_" + String(month) + ".txt";
  File file = SD.open(filename, FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available() && count < maxEntries) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() < 16) continue;

    CalendarEntry e;
    e.year = line.substring(0, 4).toInt();
    e.month = line.substring(5, 7).toInt();
    e.day = line.substring(8, 10).toInt();
    e.hour = line.substring(11, 13).toInt();
    e.minute = line.substring(14, 16).toInt();
    e.title = line.substring(17);
    entries[count++] = e;
  }
  file.close();
  return count;
}

// ========== Save entry ==========
void saveEntry(const CalendarEntry &entry) {
  String filename = "/calendar/" + String(entry.year) + "_" +
                    String(entry.month) + ".txt";

  File file = SD.open(filename, FILE_APPEND);
  if (!file) {
    einkPrint("Error: File");
    return;
  }

  String line = String(entry.year) + "-" +
                (entry.month < 10 ? "0" : "") + String(entry.month) + "-" +
                (entry.day < 10 ? "0" : "") + String(entry.day) + " " +
                (entry.hour < 10 ? "0" : "") + String(entry.hour) + ":" +
                (entry.minute < 10 ? "0" : "") + String(entry.minute) + " " +
                entry.title + "\n";

  file.print(line);
  file.close();
}

// ========== OLED preview while scrolling ==========
void showPreviewMonthOLED() {
  String line = String(previewYear % 100);
  line += "/";
  if (previewMonth < 10) line += "0";
  line += String(previewMonth);

  clearOled();
  oledPrint(line);
  oledPrint("ENTER = display");
}

// This function is called, for example, by the touchpad handler
void scrollMonth(bool up) {
  if (up) {
    previewMonth++;
    if (previewMonth > 12) {
      previewMonth = 1;
      previewYear++;
    }
  } else {
    previewMonth--;
    if (previewMonth < 1) {
      previewMonth = 12;
      previewYear--;
    }
  }
  showPreviewMonthOLED();
}

// ========= Load tasks from file =========
int loadTasks(TaskEntry* tasks, int maxTasks) {
  File file = SD.open("/tasks.txt", FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available() && count < maxTasks) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() < 10) continue;

    int p1 = line.indexOf('|');
    int p2 = line.indexOf('|', p1 + 1);
    if (p1 == -1 || p2 == -1) continue;

    String name = line.substring(0, p1);
    String date = line.substring(p1 + 1, p2);

    if (date.length() != 10) continue; // Format YYYY-MM-DD

    TaskEntry t;
    t.name = name;
    t.year = date.substring(0, 4).toInt();
    t.month = date.substring(5, 7).toInt();
    t.day = date.substring(8, 10).toInt();

    tasks[count++] = t;
  }

  file.close();
  return count;
}

// ========== Smart refresh: every 5th is full ==========
void einkClearSmart() {
  einkRefreshCount++;
  if (einkRefreshCount >= 5) {
    einkRefreshCount = 0;
    clearEink(true);  // true = full refresh
  } else {
    clearEink(false); // false = partial refresh
  }
}

// Show current scroll block on OLED
void showWeekBlockOnOled() {
  clearOled();
  if (weekTimeBlock == 0) oledPrint("Night");
  else if (weekTimeBlock == 1) oledPrint("Morning");
  else if (weekTimeBlock == 2) oledPrint("Afternoon");
  else oledPrint("Evening");
}

// ========== Draw day view ==========
void drawDay(int year, int month, int day) {
  einkClearSmart();
  einkPrint("Day " + String(day) + "/" + String(month));

  CalendarEntry entries[50];
  int entryCount = loadEntries(year, month, entries, 50);

  TaskEntry tasks[50];
  int taskCount = loadTasks(tasks, 50);

  int startHour = 0;
  if (weekTimeBlock == 1) startHour = 6;
  else if (weekTimeBlock == 2) startHour = 13;
  else if (weekTimeBlock == 3) startHour = 20;
  int endHour = startHour + 5;

  einkPrint("Block: " + String(startHour) + ":00-" + String(endHour + 1) + ":00");

  for (int h = startHour; h <= endHour; h++) {
    for (int part = 0; part < 2; part++) {
      String line = (part == 0 ? (h < 10 ? "0" : "") + String(h) + ":" : "    ");

      bool hasContent = false;

      for (int i = 0; i < taskCount; i++) {
        if (tasks[i].year == year && tasks[i].month == month && tasks[i].day == day) {
          line += " [Task] " + tasks[i].name;
          hasContent = true;
          break;
        }
      }

      for (int i = 0; i < entryCount; i++) {
        if (entries[i].year == year && entries[i].month == month && entries[i].day == day) {
          int rowH = entries[i].hour;
          int rowPart = (entries[i].minute >= 30 ? 1 : 0);
          if (rowH == h && rowPart == part) {
            line += " " + entries[i].title;
            hasContent = true;
          }
        }
      }

      if (hasContent) einkPrint(line);
    }
  }

  showWeekBlockOnOled();
}

// ========== Get today's date ==========
void getToday(int &year, int &month, int &day) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    year = currentYear;
    month = currentMonth;
    day = 1;
    return;
  }
  year = timeinfo.tm_year + 1900;
  month = timeinfo.tm_mon + 1;
  day = timeinfo.tm_mday;
}

int getCurrentHour() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return 12;
  return timeinfo.tm_hour;
}

// ========== Jump to today's date ==========
void jumpToToday() {
  int y, m, d;
  getToday(y, m, d);

  currentYear = y;
  currentMonth = m;

  int weekday = weekdayFromDate(y, m, d);
  currentWeek = ((d - 1) / 7) + 1;

  int hour = getCurrentHour();
  if (hour < 6) weekTimeBlock = 0;
  else if (hour < 13) weekTimeBlock = 1;
  else if (hour < 20) weekTimeBlock = 2;
  else weekTimeBlock = 3;

  calendarMode = 2;
  drawDay(y, m, d);
}
