# Smart Library — Setup Guide

## 1. Wiring (confirmed for Arduino Mega)
| Function | Pin |
|---|---|
| Ultrasonic trigger (shared) | A1 |
| Echo — Slot 0 / 1 / 2 / 3 / 4 | A2, A3, A4, A5, A6 |
| LED — Slot 0 / 1 / 2 / 3 / 4 | 2, 3, 4, 5, 6 |
| Buzzer | A7 |

On an Uno/Nano, A6/A7 can't do digital I/O — but you're on a Mega, so this wiring works as-is.

## 2. Flash the Arduino
Open `arduino/smart_library.ino` in the Arduino IDE, select **Board: Arduino Mega**, select the correct port, and upload.

Open the Serial Monitor at **9600 baud** — you should see `READY` printed once.

**Calibrate `PRESENCE_THRESHOLD_CM`** (top of the .ino file, default 15 cm): put a book in front of a sensor, note the distance at which it should count as "present" vs "empty", and adjust so it reliably tells the two apart for your shelf depth.

## 3. Install the PC software
```bash
cd software
pip install pyserial
python app.py
```

## 4. Using it
1. **Connect** — pick the Arduino's COM port and click Connect.
2. **Login** — enter a library ID from `data/users.json` (e.g. `L1001`).
3. **Category** — choose Fiction or Science.
4. **Pick a book** — each book shows its description and live availability. Selecting one lights that shelf's LED.
5. **Take the book** — if you take the right one, you get a success message. If you take the wrong one, the buzzer sounds and the screen tells you which shelf to put it back on.

## How it talks (serial protocol, 9600 baud)
```
PC -> Arduino
  STATUS            ask for availability of all 5 slots
  SELECT:<n>        user picked the book in slot n (0-4)
  RESET             cancel selection, turn off LEDs/buzzer

Arduino -> PC
  READY                 sent once at startup
  STATUS:1,1,0,1,1      1 = book present, 0 = empty
  TAKEN:<n>             correct book removed
  WRONG:<n>             a book was removed from the wrong slot
  RETURNED:<n>          a book was placed back
```

## Extending it
This prototype has 5 physical slots, so `data/books.json` has exactly 5 books (one per slot, `"slot": 0-4`). To catalogue more books than you have sensors for, you'd add a status like `"in_storage"` for books not currently on a sensored shelf, and only assign `"slot"` to whichever 5 are physically out. The software and firmware don't need to change for that — just how you manage the JSON data.

## Notes / things worth improving later
- **Multiple copies**: right now each slot = one unique book. If you want multiple copies of the same title, you'd map several slots to the same book ID.
- **Persistence**: user/book data is a flat JSON file, fine for a prototype/demo. For a real deployment, swap `load_json`/manual edits for a small SQLite database.
- **Security**: library ID login has no password — fine for a prototype, but add a PIN or card/RFID reader for anything real.
