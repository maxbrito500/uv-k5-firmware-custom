/*
 * -----------------------------------------------------------------------------
 *  GEOGRAM — Beep-Based Transmission Protocol with Blossom Filtering
 * -----------------------------------------------------------------------------
 *
 *  This protocol uses amplitude and timing of microphone input to detect a
 *  calibration sequence followed by a stream of data beeps.
 *
 *  Tone sequence:
 *
 *      Time ──────────────────────────────────────────────────────────────▶
 *
 *      ┌────────────┐         ┌───────┐         ┌──────┐       data beeps...
 *      │   HIGH     │         │  MID  │         │ LOW  │
 *      └────────────┘         └───────┘         └──────┘
 *           ▲                     ▲                 ▲
 *        Calibrate            Calibrate          Calibrate
 *
 *   - HIGH:  >300ms beep, used to trigger VOX and record HIGH average
 *   - MID:   short beep, calibrates MID average + interval
 *   - LOW:   calibrates LOW average
 *   - DATA:  beeps classified using these thresholds
 *   - END:   protocol resets after 2 seconds of silence
 * -----------------------------------------------------------------------------
 */

#include "driver/bk4819.h"
#include "ui/helper.h"
#include <stddef.h>  // for size_t
#include <app/flashlight.h>
 
 #define MIC_THRESHOLD             1000
 #define HIGH_MIN_DURATION         150   // 100ms
 #define SILENCE_TIMEOUT           200  // 2s = 200 * 10ms
 #define BLOSSOM_ALPHA             0.2f
 #define BLOSSOM_TOLERANCE_PCT     10
 
 typedef enum {
     STATE_WAIT_FOR_HIGH,
     STATE_WAIT_FOR_MID,
     STATE_WAIT_FOR_LOW,
     STATE_TRANSMISSION
 } ProtocolState;
 
 static ProtocolState state = STATE_WAIT_FOR_HIGH;
 
 static uint32_t gGeogramTime = 0;
 static uint32_t maxTimeCountBeforeReset = 864000000; // 100 days
 static uint32_t beepStartTime = 0;
 static uint32_t lastBeepEndTime = 0;
 static uint16_t filteredMic = 0;
 
 static uint32_t highAvg = 0, midAvg = 0, lowAvg = 0;
 static uint8_t  highCount = 0, midCount = 0, lowCount = 0;
 
 //static uint32_t beepInterval = 0;
 
 // Display a label like "MID: 0942"
 static void printLabelWithValue(const char *label, uint16_t value) {
    char buf[24];  // Enough for long label + ": " + 4 digits + null terminator
    char *p = buf;

    // Copy label into buffer (limit total length)
    while (*label && (size_t)(p - buf) < sizeof(buf) - 7) {
        *p++ = *label++;
    }

    // Add ": " separator
    *p++ = ':';
    *p++ = ' ';

    // Append 4-digit zero-padded number
    *p++ = '0' + (value / 1000) % 10;
    *p++ = '0' + (value / 100) % 10;
    *p++ = '0' + (value / 10) % 10;
    *p++ = '0' + value % 10;
    *p = '\0';  // Null-terminate

    // Compute display X position based on string length (6 pixels per char, max 127px)
    uint8_t x = 127;
    const uint8_t charWidth = 6;
    uint8_t len = (uint8_t)(p - buf);
    if (len * charWidth <= 127)
        x = 127 - len * charWidth;

    UI_PrintStringSmallNormal(buf, 0, x, false);
}


 
 // Blossom filter: smooths mic input and ignores outliers
 static uint16_t applyBlossomFilter(uint16_t micLevel) {
     uint16_t lower = filteredMic * (100 - BLOSSOM_TOLERANCE_PCT) / 100;
     uint16_t upper = filteredMic * (100 + BLOSSOM_TOLERANCE_PCT) / 100;
 
     if (micLevel >= lower && micLevel <= upper) {
         filteredMic = (uint16_t)(BLOSSOM_ALPHA * micLevel + (1.0f - BLOSSOM_ALPHA) * filteredMic);
     } else {
         filteredMic = (filteredMic + micLevel) / 2;
     }
 
     return filteredMic;
 }

 // Reset the blossom filter to its initial state
 static void resetBlossomFilter() {
     filteredMic = 0;
 }
 
 
 static void transitionTo(ProtocolState newState) {
     state = newState;
 }
 
 void printLabelStatus(uint16_t micLevel) {
     switch(state) {
         case STATE_WAIT_FOR_HIGH:
            printLabelWithValue("WAIT_HIGH", micLevel);
             break;
         case STATE_WAIT_FOR_MID:
            printLabelWithValue("WAIT_MED", micLevel);
             break;
            case STATE_WAIT_FOR_LOW:
            printLabelWithValue("WAIT_LOW", micLevel);
             break;
         case STATE_TRANSMISSION:
            printLabelWithValue("WAIT_TRANS", micLevel);
             break;
         default:
            printLabelWithValue("SND", micLevel);
             break;
     }
 }

 // there is a beep, handle it
void handleBeepOn(uint16_t micLevel) {
    // start counting the time when it hasn't started before
    if(beepStartTime == 0){
        beepStartTime = gGeogramTime;
        resetBlossomFilter();
    }

    // avoid outlier sounds
    uint16_t smoothLevel = applyBlossomFilter(micLevel);
    printLabelStatus(smoothLevel);

    // Accumulate values to average later
    switch (state) {
        case STATE_WAIT_FOR_HIGH:
            if(smoothLevel > highAvg) {
                highAvg = smoothLevel;
            }
            highCount++;
            break;
        case STATE_WAIT_FOR_MID:
            midAvg = smoothLevel;
            midCount++;
            break;
        case STATE_WAIT_FOR_LOW:
            lowAvg = smoothLevel;
            lowCount++;
            break;
        default:
            break;
     }
 }

// there is no beep, handle it
void handleBeepOff() {
    // No sound detected before
    if(beepStartTime == 0){
        return; // No previous beep to handle
    }
    

    // Sound just ended, count how long it lasted
    lastBeepEndTime = gGeogramTime;
    uint32_t duration = lastBeepEndTime - beepStartTime;
    printLabelWithValue("DUR", duration);

    switch (state) {
        case STATE_WAIT_FOR_HIGH:
            if (duration >= HIGH_MIN_DURATION && highCount > 0) {
                printLabelWithValue("HIGH", highAvg);
                transitionTo(STATE_WAIT_FOR_MID);
                ACTION_FlashLight(); // Turns it ON
            }
            break;
            
        default:
            break;
        }


        // reset the beep
        beepStartTime = 0;
        lastBeepEndTime = 0;
    }
    

/*
        case STATE_WAIT_FOR_MID:
            if (midCount > 0) {
                avgMid = midAvg / midCount;
                printLabelWithValue("MID", avgMid);
                beepInterval = beepStartTime - lastBeepEndTime;
                lastBeepEndTime = gGeogramTime;
                transitionTo(STATE_WAIT_FOR_LOW);
            }
            break;

        case STATE_WAIT_FOR_LOW:
            if (lowCount > 0) {
                avgLow = lowAvg / lowCount;
                printLabelWithValue("LOW", avgLow);
                lastBeepEndTime = gGeogramTime;
                transitionTo(STATE_TRANSMISSION);
            }
            break;

        case STATE_TRANSMISSION:
            lastBeepEndTime = gGeogramTime;
            classifyData(filteredMic);
            break;
            */
  

    // Reset accumulators after beep ends
    //highAvg = midAvg = lowAvg = 0;
    //highCount = midCount = lowCount = 0;



 void GEOGRAM_Hook(void) {
    gGeogramTime++;
    // make sure we don't overflow in the time counter
    if (gGeogramTime > maxTimeCountBeforeReset) {
        gGeogramTime = 0;
    }

    // get the volume level right now
    uint16_t micLevel = BK4819_ReadRegister(0x64) & 0x7FFF;
    // are we above the minimum sound level?
    bool isSounding = (micLevel > MIC_THRESHOLD);

    // there is valid sound
    if(isSounding){
        handleBeepOn(micLevel);
    }else{
        handleBeepOff();
    }

}

     //////////////////////////////////////

     /*
     if (isSounding) {
         UI_PrintStringSmallNormal("HEARING", 0, 80, false);
 
         uint16_t smoothLevel = applyBlossomFilter(micLevel);
 
         if (!inBeep) {
             // This is the start of a new beep
             inBeep = true;
             beepStartTime = gGeogramTime;
         }
 
         // Accumulate values to average later
         switch (state) {
             case STATE_WAIT_FOR_HIGH:
                 highSum += smoothLevel;
                 highCount++;
                 break;
             case STATE_WAIT_FOR_MID:
                 midSum += smoothLevel;
                 midCount++;
                 break;
             case STATE_WAIT_FOR_LOW:
                 lowSum += smoothLevel;
                 lowCount++;
                 break;
             default:
                 break;
         }
 
     } else if (inBeep) {
         // Sound just ended
         inBeep = false;
         uint32_t duration = gGeogramTime - beepStartTime;
 
         switch (state) {
             case STATE_WAIT_FOR_HIGH:
                 if (duration >= HIGH_MIN_DURATION && highCount > 0) {
                     avgHigh = highSum / highCount;
                     printLabelWithValue("HIGH", avgHigh);
                     lastBeepEndTime = gGeogramTime;
                     transitionTo(STATE_WAIT_FOR_MID);
                 }
                 break;
 
             case STATE_WAIT_FOR_MID:
                 if (midCount > 0) {
                     avgMid = midSum / midCount;
                     printLabelWithValue("MID", avgMid);
                     beepInterval = beepStartTime - lastBeepEndTime;
                     lastBeepEndTime = gGeogramTime;
                     transitionTo(STATE_WAIT_FOR_LOW);
                 }
                 break;
 
             case STATE_WAIT_FOR_LOW:
                 if (lowCount > 0) {
                     avgLow = lowSum / lowCount;
                     printLabelWithValue("LOW", avgLow);
                     lastBeepEndTime = gGeogramTime;
                     transitionTo(STATE_TRANSMISSION);
                 }
                 break;
 
             case STATE_TRANSMISSION:
                 lastBeepEndTime = gGeogramTime;
                 classifyData(filteredMic);
                 break;
         }
 
         // Reset accumulators after beep ends
         highSum = midSum = lowSum = 0;
         highCount = midCount = lowCount = 0;
     }

     
 
     // Reset protocol if silent too long in transmission mode
     if (state == STATE_TRANSMISSION &&
         (gGeogramTime - lastBeepEndTime) >= SILENCE_TIMEOUT) {
         UI_PrintStringSmallNormal("END", 0, 80, false);
         transitionTo(STATE_WAIT_FOR_HIGH);
     }
         */

 