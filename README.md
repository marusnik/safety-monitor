# Safety Monitor

A human-aware safety monitor running on a Raspberry Pi 5. A camera watches a space, a pretrained
neural network detects people, and the system decides **CLEAR / SLOW / STOP** for a nearby
machine, the way a robot's safety channel would.

The machine-learning detector is treated as a component that **can be wrong**: its output is
wrapped in runtime checks, cross-checked against an independent non-ML distance sensor, and the
system falls back to **STOP** whenever it is uncertain or its inputs go stale.

> **Why this project:** I spent 15 years writing DO-178B certified avionics software at Garmin.
> This project applies the same discipline (requirements, verification, fail-safe design) to an
> ML-based perception system on Linux, using modern C++.

## Status

| Week | Topic | Status |
|---|---|---|
| 1 | Raspberry Pi setup, CMake, webcam capture in C++ | Done |
| 2 | Two-thread capture pipeline with a thread-safe queue | Done (thread sanitizer pending, see Known issues) |
| 3 | Pretrained person detector (OpenCV DNN) | Done (one open question, see Open questions) |
| 4 | Distance sensor (VL53L1X over I2C), zone logic with hysteresis | Done (no recovery after a sensor fault yet, see Known issues) |
| 5 | ML safety monitor: confidence intervals, plausibility checks, fail-safe STOP | Planned |
| 6 | Cross-checking the camera against the distance sensor | In progress (combined decision done; latched fault and reset pending) |
| 7 | Rare-scenario validation set (low light, occlusion, ...) | Planned |
| 8 | Profiling, sanitizers, CI | Planned |
| 9 | Requirements traced to tests, safety argument | Planned |
| 10 | Documentation and demo | Planned |

## Hardware

- Raspberry Pi 5 (8 GB), active cooler, 27 W USB-C power supply
- Microsoft LifeCam HD-5001 USB webcam (UVC, appears as `/dev/video0`)
- VL53L1X time-of-flight distance sensor on I2C bus 1 (`/dev/i2c-1`, address 0x29)

## Software

- Raspberry Pi OS (64-bit)
- C++20, g++ 14, CMake 3.31
- OpenCV 4.10 (`libopencv-dev`)
- ST VL53L1X Ultra Lite Driver (ULD) v3.5.5 in `third_party/vl53l1x_uld/`, unmodified
  (ST's BSD license, see `third_party/vl53l1x_uld/LICENSE.txt`). It talks to the sensor through
  a Linux platform adapter (`src/vl53l1_platform.cpp`) built on `I2cDevice`.

## Camera configuration

The program configures the camera at startup, so the settings are part of the code rather than
something to remember:

| Setting   | Value              | Why                                                               |
|-----------|--------------------|-------------------------------------------------------------------|
| Device    | `/dev/video0`, V4L2 | LifeCam HD-5001                                                  |
| Autofocus | Off                | Autofocus "hunts" when the scene changes, which is exactly when a person walks in; a fixed focus makes the camera behave the same way every run |
| Focus     | 11 (`focus_absolute`) | Chosen by hand: subject sharpest at 1–2 m, the distance range that matters for SLOW and STOP |

**The settings are verified, not trusted.** Some drivers report success without applying a
setting, so after setting them the program reads both values back (as integers) and compares
them with what it requested:

- If setting a value reports failure, the program prints a warning and still checks the read-back.
- **If the read-back does not match, the monitor refuses to start.** Validation results only apply
  to the camera configuration they were measured with; a camera in an unknown configuration
  would make them meaningless.

The settings can also be checked from outside while the program runs:

```bash
v4l2-ctl -d /dev/video0 --get-ctrl=focus_absolute,focus_automatic_continuous
```

**Not yet measured:** detection with autofocus on versus off. Planned as part of the week 7
validation runs.

### Decision: auto-exposure stays on

| Option              | In dim light                                    | Risk                         |
|---------------------|-------------------------------------------------|------------------------------|
| Auto-exposure on    | Image stays bright; frame rate drops (27 → 7 FPS) | Slower reaction, motion blur |
| Fixed exposure      | Frame rate stays constant; image gets dark      | **Missed people**            |

**Auto-exposure stays on**, because a missed person is the worst failure this system can have
(the same reasoning as the low confidence threshold). The cost is a slower camera in low light,
so the reaction time must be budgeted for the worst case rather than the best one.

**Worst-case reaction time in dim light (estimate):**

| Delay                                                  | ~7.9 FPS (dim)   |
|--------------------------------------------------------|------------------|
| Waiting for the next frame (worst case: one interval)  | ~127 ms          |
| Camera exposure and transfer (not measured, estimated) | ~50–100 ms       |
| Detection                                              | ~80 ms           |
| **Total**                                              | **~260–310 ms**  |

Machine-safety standards use an approach speed of 1.6 m/s for a walking person (ISO 13855). At
that speed a person covers **~0.4–0.5 m** before the monitor reacts, nearly the whole current
STOP distance (500 mm). Consequences for the design:

1. **Zone distances will be derived from the worst-case reaction time**, not chosen as round
   numbers: `safety distance = approach speed × reaction time + margin`.
2. **A frame rate below the minimum assumed in that calculation is a fault:** the monitor is
   outside its design envelope and goes to a degraded state (SLOW or STOP).
3. **Image brightness will also be monitored** (mean of the grayscale frame). Frames outside the
   brightness range covered by validation are treated the same way.

The minimum frame rate and the brightness limits will come from the week 7 measurements: the
conditions in which detection still meets the missed-person requirement. Worth trying first:
keep auto-exposure but disable the camera's frame-rate reduction (`exposure_dynamic_framerate=0`,
if supported), which raises gain instead of lengthening exposure: noisier images, constant
frame rate.

## Building

```bash
sudo apt install -y build-essential cmake git gdb libopencv-dev v4l-utils
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Running

```bash
./build/safety_monitor            # camera, detector and sensor threads for CAM_RUN_PERIOD_SEC; prints zone changes and a report
./build/detect_image frame.jpg    # runs person detection on one image, saves result.jpg
./build/range_check               # 20 sensor readings: valid count, min, max, average, failed statuses
./build/sensor_check              # reads the sensor's model ID over I2C
./build/test_frame_queue          # ring buffer tests
./build/test_thread_safe_queue    # thread-safe queue tests (two threads)
./build/test_zone_monitor         # zone logic and hysteresis tests
./build/test_safety_output        # combined camera + sensor decision tests
./build/hw_test_i2c_device        # hardware test: needs the sensor connected
```

Run from the repository root, because the detector loads its model files from `models/`.
No output from a test program means all tests passed (`hw_test_i2c_device` prints
`TEST PASSED`).

## Project structure

```
include/
  frame.hpp               Frame: image + capture timestamp + sequence number
  frame_queue.hpp         RingBuffer<T, Capacity>: fixed-size, no heap, overwrite-oldest policy
  thread_safe_queue.hpp   ThreadSafeQueue<T, Capacity>: mutex + condition variable around RingBuffer
  person_detector.hpp     PersonDetector: loads the model, returns person boxes above a threshold
  i2c_device.hpp          I2cDevice: RAII wrapper around /dev/i2c-1; register reads/writes as one I2C_RDWR transaction
  vl53l1_platform_attachment.hpp  connects ST's driver to one I2cDevice for the attachment's lifetime
  vl53l1x_api.hpp         extern "C" wrapper around ST's C header
  vl53l1x_sensor.hpp      Vl53l1xSensor: owns bus, driver attachment and ranging; read() returns status + distance
  zone_monitor.hpp        ZoneMonitor: distance -> CLEAR / SLOW / STOP with hysteresis
  zone_sample.hpp         ZoneSample: zone + timestamp in one lock-free atomic; stale reads return STOP
  camera_sample.hpp       CameraSample: person seen + capture timestamp in one lock-free atomic
  clock.hpp               steady-clock milliseconds (to_ms, now_ms), shared by both samples
  safety_output.hpp       safety_output(): combines the sensor and camera samples into one zone
src/
  main.cpp                camera, processing and sensor threads; zone printout; report
  person_detector.cpp
  detect_image.cpp        command-line tool: detect people in one image
  i2c_device.cpp
  vl53l1_platform.cpp     the platform functions ST's driver calls, implemented on I2cDevice
  vl53l1x_sensor.cpp      boot/ID/init checks, start/stop ranging, read()
  zone_monitor.cpp
  safety_output.cpp
  range_check.cpp         command-line tool: ranging accuracy runs
  sensor_check.cpp        command-line tool: model ID check
third_party/vl53l1x_uld/  ST's driver, unmodified
models/                   model files (not in git, see Models)
tests/
  test_frame_queue.cpp
  test_thread_safe_queue.cpp
  test_zone_monitor.cpp
  test_safety_output.cpp
  hw_test_i2c_device.cpp  write, read back and restore a real sensor register
```

## Design

```
[camera thread]                        [processing thread]
read into a fresh Frame                pop_wait(frame, 500 ms)
stamp captured_at + sequence    --->   detect people (SSD MobileNet v2, OpenCV DNN)
push (drop oldest if full)             on TIMEOUT: report "camera silent"
```

- **Fixed-size ring buffer, no dynamic allocation in the queue.** When full, the oldest frame
  is overwritten: for a monitor, a stale frame is worse than a dropped one.
- **Depth-1 queue ("mailbox").** The processing thread always receives the newest frame
  (see the latency findings below for why).
- **`pop_wait` has a timeout.** No frame within 500 ms means the camera has gone silent, and
  that is treated as a fault, not a pause. Absence of data is a hazard.
- **Each statistics counter has exactly one writer thread**, so no locks are needed for them;
  `main()` reads them only after `join()`.
- **Clean shutdown:** an `std::atomic<bool>` stops the camera thread, which closes the queue;
  the processing thread drains remaining frames before exiting, so no captured frame is lost.

### Distance sensor path

```
[sensor thread]                                    [reader]
read(): wait for data (max ~200 ms)                load ZoneSample
status 0 -> distance, else nullopt          --->   older than 500 ms? -> STOP
ZoneMonitor.update() -> zone                       else -> sample.zone
store ZoneSample { zone, now_ms }
```

**Any sensor failure ends in STOP.** Failures are caught at three levels:

1. **Startup.** The sensor must boot within 1 s, report the expected ID (0xEACC) and pass ST's
   init before any thread starts, because a dead or wrong device must fail while the program
   can still exit cleanly. A startup failure exits with code 1.
2. **Every read.** An I2C error, no data within ~200 ms, or a range status other than 0 gives no
   distance, and no distance means STOP. Valid is not the same as accurate (finding 9), so the
   zones still need a margin.
3. **A hung thread.** The zone is published together with its timestamp in one lock-free atomic,
   so a reader can never pair a new zone with an old time. Readers treat a sample older than
   500 ms as STOP, because a hung thread can't report its own failure. The initial sample is
   already stale, so "never published" and "stopped publishing" are the same case.

Other design points:

- **One thread owns the driver.** Only the sensor thread calls `read()`, so ST's driver never
  runs on two threads at once.
- **RAII for the hardware:** `Vl53l1xSensor` owns the bus, the driver attachment and the ranging
  session. Members are declared in that order, so they start in that order and stop in reverse:
  ranging stops before the bus closes.
- **The interrupt is cleared even after a failed read,** because otherwise an old result can look
  new on the next read.
- **Readings are logged only on change** (first failure, different error, recovery): one line
  per retry would hide the line that matters, and writing to `cerr` that often can affect timing.
- **Zones:** STOP below 500 mm, SLOW below 1500 mm, CLEAR beyond, with 100 mm hysteresis
  (STOP is released at 600 mm, SLOW at 1600 mm), so a person standing on a boundary doesn't
  make the output flicker. These are placeholder values until they are derived from the reaction
  time (see the camera section above).

### Combining camera and sensor (week 6)

```
[processing thread]                         [main loop, every 100 ms]
after each detection:                       load ZoneSample + CameraSample
store CameraSample { person, captured_at }  safety_output(sensor, camera, now) -> zone
on TIMEOUT: store nothing                   print zone changes
```

`safety_output()` is a pure function with the time passed in, so every rule can be unit tested
without threads or hardware. Its rules, in order:

| Condition | Output |
|-----------|--------|
| Sensor sample older than 500 ms | STOP |
| Camera sample older than 500 ms | STOP |
| Sensor says STOP | STOP |
| Camera sees a person, sensor says CLEAR or SLOW | SLOW |
| Otherwise | the sensor's zone |

- **Both staleness checks come first,** because a stale value is not evidence of anything. A
  stale "person present" must not be allowed to produce SLOW (finding 12).
- **The camera sample is stamped with `captured_at`, not the time detection finished,** because
  a detection describes the scene when the image was taken. Its age therefore includes the
  detection time (~80 ms).
- **On a camera timeout nothing is published,** so camera loss is handled by staleness, the same
  way as a hung sensor thread. The initial camera sample is already stale.

Design decisions:

- **Either channel reporting danger wins** (one-out-of-two), because each can miss a person the
  other sees: the sensor's field of view is narrow, and the camera has no distance.
- **Camera sees a person, sensor says CLEAR or SLOW: SLOW, not STOP,** because the camera
  reports people anywhere in view, including far outside the zone. STOP for every person in view
  would make the system stop so often that operators would work around it.
- **Residual risk:** a person close to the machine but outside the sensor's cone is seen only by
  the camera, so the output is SLOW instead of STOP.
- **After a persistent sensor fault, STOP latches until a manual reset**, because someone may
  have entered the area during the fault, and the cause should be checked before the machine
  runs again. A single failed read gives STOP without latching, because a latch on every glitch
  would hurt availability. "Persistent" will be defined as a duration, not a count, because a
  failed read takes ~10 ms or ~200 ms depending on how it fails. Not built yet.

## Findings

### 1. Frame rate depends on lighting (auto-exposure)

| Condition | Measured FPS |
|-----------|--------------|
| Dim room  |   7.3        |
| Lamp on   | ~27          |

In low light, the webcam lengthens each frame's exposure time, which caps the frame rate.
For a safety monitor, frame rate is reaction time: at 7 FPS a walking person moves roughly
20 cm between frames. **Low light is therefore a degraded condition the monitor must detect.**
(Planned as a week 7 test case.)

### 2. Copying a `cv::Mat` shares the pixels

Reading repeatedly into the same `cv::Mat` and pushing it into the queue produced
**four queue entries with the same pixel address**: four frames with different timestamps and
sequence numbers, but one image. Copying a `cv::Mat` copies only its header (a pointer to the
pixels). Fix: read into a fresh `Frame` (fresh `cv::Mat`) every iteration, so each image owns
its own buffer. With two threads this is also what prevents a data race on the pixels.

### 3. Pipeline latency, and what it does not include

Latency is measured from `captured_at` (when `cap.read()` returns) to the end of processing.

| Run (10 s)            | Captured FPS | Processed FPS | Dropped | Avg latency | Max latency |
|-----------------------|--------------|---------------|---------|-------------|-------------|
| Grayscale + blur only | 21.4         | 21.4          | 0       | 1.0 ms      | 1.4 ms      |

**Not included:** the camera's own exposure, internal processing, and USB transfer, which
happen before `read()` returns. The true end-to-end reaction time is larger and must be
budgeted separately.

### 4. Queue depth adds latency when the consumer is slow

With a simulated 100 ms detector:

| Queue depth | Captured FPS | Processed FPS | Dropped | Avg latency | Max latency |
|-------------|--------------|---------------|---------|-------------|-------------|
| 4           | 19.0         | 9.2           | 98      | 183 ms      | 470 ms      |
| 1           | 16.5         | 10.1          | 64      | 131 ms      | 173 ms      |

With depth 4 and a slow consumer, the queue stays full and the consumer keeps receiving the
**oldest** buffered frame. Model:

> latency ≈ (age of the frame when popped) + processing time

- Depth 4: the popped frame has waited ~3 frame intervals (~160 ms), plus 100 ms of processing.
- Depth 1: the popped frame is between 0 and 1 frame interval old (~30 ms on average at
  16.5 FPS), plus 100 ms of processing ≈ 130 ms. **Measured: 131 ms.**

**Decision:** depth 1 for the safety path. Deeper queues are right for recording every frame,
not for acting on the newest one. In every run, `captured == processed + dropped` held exactly.

### 5. The first inference is about twice as slow as the rest

Person detection with SSD MobileNet v2 (300 × 300 input, OpenCV DNN, CPU only):

| Measurement                                         | Time    |
|-----------------------------------------------------|---------|
| First `forward()` call                              | ~190 ms |
| Steady state (average of 10 runs after one warm-up) | ~ 92 ms |

The first call includes one-time setup (memory allocation, layer preparation). Timing a single
call overstates the detector's cost by about 2×, so the pipeline runs one warm-up detection
before starting the threads.

At ~92 ms per frame, the detector can process at most ~10–11 FPS, slower than the camera's
~20 FPS. The detector is the bottleneck, which makes the depth-1 queue from finding 4 essential.

### 6. Which component is the bottleneck depends on the lighting

Full pipeline with the real detector, depth-1 queue, 10 s runs:

| Light              | Captured FPS | Processed FPS | Dropped | Avg latency | Max latency | Bottleneck |
|--------------------|--------------|---------------|---------|-------------|-------------|------------|
| Dim                | 7.9          | 7.7           | 2       | 82 ms       | 160 ms      | Camera     |
| Lamp on            | 20.0         | 10.5          | 95      | 92 ms       | 155 ms      | Detector   |
| Bright (at a lamp) | 29.2         | 12.1          | 171     | 99 ms       | 162 ms      | Detector   |

- **Dim:** frames arrive every ~127 ms but detection takes ~80 ms, so the detector waits for
  each frame and takes it almost immediately. Almost nothing is dropped, and latency is close
  to the detection time alone.
- **Bright:** frames arrive faster than the detector can process them; the depth-1 queue drops
  the extra frames so the detector always gets the newest one.

The system's behavior depends on the environment, and the safety analysis must cover both
regimes. In every run, `captured == processed + dropped` held exactly.

### 7. Latency breakdown: frame age + detection time

The report splits pipeline latency into the frame's age when the detector picks it up and the
detection time itself:

| Run         | Frame interval | Avg age at pop | Avg detection | Age + detection | Pipeline latency |
|-------------|----------------|----------------|---------------|-----------------|------------------|
| 14.0 FPS    | 71 ms          | 7.3 ms         | 80.2 ms       | 87.5 ms         | 87.6 ms          |
| 29.2 FPS    | 34 ms          | 15.8 ms        | 82.7 ms       | 98.5 ms         | 98.6 ms          |

- **The two parts add up to the measured latency** in both runs: the instrumentation accounts
  for all of it, with no unmeasured delay in between.
- **At 29.2 FPS the age matches the model:** the newest frame is between 0 and one interval
  old, ~17 ms on average; measured 15.8 ms.
- **At 14 FPS it does not:** expected ~35 ms, measured 7.3 ms. Still unexplained, see
  Open questions.

### 8. Detection results so far

| Scene                                 | Frames with a person detected |
|---------------------------------------|-------------------------------|
| Person in view, dim light (run 1)     | 68 / 77                       |
| Person in view, dim light (run 2)     | 77 / 77                       |
| Person in view, lamp on               | 105 / 105                     |
| Empty scene, camera pointed at a lamp | 0 / 121                       |

- **Misses vary from run to run:** 9 missed frames (~12%) in one run, none in the next, at the
  same threshold (0.3). A person does not vanish for one frame, so the safety monitor must not
  release STOP on a single empty frame (planned for week 5).
- **No false positives** in a very bright, nearly overexposed empty scene.
- These are informal runs, not a validation set. Systematic missed-detection and
  false-positive rates come in week 7.

### 9. VL53L1X ranging accuracy (one room, one run per case, tape reference)
- "Valid" does not mean accurate: with a physical table in the sensor's field of view,
  status 0 readings were about 41 mm short at 300 mm. Raising the sensor halved the error.
- Fixed targets up to 1600 mm (wall and dark blue shirt): individual readings were
  -23 to +11 mm from the tape. No single offset fits, so the design uses a margin
  (provisional +/-30 mm) rather than calibration.
- A black shirt was valid at every distance tested (500-1600 mm).
- A wall was valid out to 4000 mm. At 5000 mm, all readings were status 2 (signal failed).
- Invalid readings map to STOP, so the monitor holds STOP if nothing is within range.
- Not yet tested: direct sunlight, angled targets, other materials, repeatability across days.
- After moving the sensor code into `Vl53l1xSensor`, `range_check` at 300 / 500 mm read
  276.6 / 477.1 mm, within ~3 mm of the earlier runs: the refactor did not change the readings.

### 10. End-to-end sensor path

With all three threads running and a person moving in front of the sensor:

- The initial stale STOP was replaced by the first real zone ~100 ms after start, which suggests
  one reading every ~100 ms (to be confirmed from the sensor's timing budget).
- Zones followed the movement, with the 100 mm hysteresis visible at both boundaries.
- A hand waved closer than 500 mm went straight from CLEAR to STOP and back, as designed.
- All threads stopped and joined cleanly (exit code 0).

### 11. Unplug tests: the failure paths, seen working

| Test | What happened | Result |
|------|---------------|--------|
| Sensor unplugged mid-run | I2C errors (`0xFF`, the adapter's -1); **STOP 38 ms after the first failed read** | Fails safe |
| Sensor plugged back in | The bus answers again, but the sensor has reset to its defaults and never reports data ("not ready in 200 ms") | Stays STOP: safe, but no recovery |
| Camera unplugged mid-run (before week 6) | `cap.read()` returned false immediately (it did not block); "camera silent" logged | Detected, **but the zone kept following the sensor**: camera loss did not feed STOP (fixed, see finding 12) |
| Camera plugged back in (about 10 times) | Never recovered; `ENODEV` (no such device) warning at shutdown | No recovery: the open handle points to a device that is gone |

The sensor results confirm the design above. The camera results show the biggest gap left: for
about 47 s the system ran with no camera and still reported SLOW.

### 12. Camera loss now gives STOP

Same test as finding 11, after adding `CameraSample` and `safety_output()`: a person in front
of both channels (output SLOW), then the camera unplugged and left out.

| Run | What happened | Result |
|-----|---------------|--------|
| 1 | Output stayed SLOW for ~33 s with a stale camera, and changed to STOP only when the sensor itself said STOP | **Bug:** a copy-paste error let the stale "person present" sample reach the SLOW rule |
| 2 (fixed) | **STOP while the sensor was fresh and said SLOW**, 42 ms before the 500 ms "camera silent" log | Fails safe |

- **STOP comes before the "camera silent" log, as expected,** because the two timers start at
  different points: staleness counts from `captured_at`, the pop timeout from the last pop,
  which comes after the queue wait and ~80 ms of detection.
- **Worst-case reaction is ~600 ms, not 500 ms:** 500 ms staleness from the last image, plus up
  to 100 ms until the main loop next checks.
- **The unit test would have caught the bug, but it wasn't run before the hardware test.** It
  also had a weakness: the test setup sets the sensor to STOP, so the stale-camera case returned
  STOP from the sensor and never reached the camera check. Each test now needs to be checked by
  removing the rule it covers and seeing it fail. Running the tests on every build (CTest) is
  planned for week 8.

## Models

| Item         | Value                                                                             |
|--------------|-----------------------------------------------------------------------------------|
| Model        | SSD MobileNet v2 (COCO, 2018-03-29)                                               |
| Weights      | `frozen_inference_graph.pb`, TensorFlow model zoo                                 |
| Config       | `ssd_mobilenet_v2_coco_2018_03_29.pbtxt`, OpenCV `opencv_extra` (`testdata/dnn/`) |
| License      | Apache 2.0                                                                        |
| Input        | 300 × 300 RGB                                                                     |
| Class used   | 1 = person                                                                        |

Model files are not stored in this repository. Download them into `models/`.

**Confidence threshold:** 0.3, provisional. For a safety monitor, a missed person is worse than
a false stop, so the threshold starts low. The final value will be chosen from the week 7
validation set against a stated missed-detection requirement, not by eye.

## Open questions

- **Frame age at 14 FPS (finding 7):** expected ~35 ms, measured 7.3 ms. Frames appeared to
  arrive just as the detector became free. One hypothesis was that the detector, which uses all
  four CPU cores, delays the camera thread so `captured_at` is stamped late (which would make
  the measured latency optimistic). The 29.2 FPS run weakens it: the camera ran at full rate
  while the detector was busy. Next experiment: `cv::setNumThreads(3)` in medium light, to see
  whether leaving one core free changes the frame age.

## Known issues

- **No recovery after a sensor or camera fault.** Both stay failed until the program restarts.
  This is safe (both give STOP), but not available. Planned: a latched fault with a manual reset
  (see the design decisions above), sensor re-init and camera re-open.
- **One missed detection drops SLOW to CLEAR immediately** (finding 8: 68 of 77 frames in dim
  light). Planned for week 5: hold "person present" for a short time after the last detection.
- **The 500 ms stale limits (sensor and camera) are provisional.** At 1.6 m/s, 500 ms is 800 mm,
  more than the whole STOP zone, and the end-to-end reaction is ~600 ms (finding 12). It must stay above the slowest normal update (the timing budget plus how long
  `read()` can block) and below what the safety distance allows. The timing budget has not been
  read from the sensor yet.
- **The millisecond timestamp is 32 bits and wraps every 49.7 days.** If the sensor thread hung
  for that long, an old sample would briefly look fresh again. A 64-bit timestamp would make
  `ZoneSample` 16 bytes, which may no longer be lock-free on the Pi (the `static_assert` would
  catch that). Alternative: stay in STOP until the timestamp changes.
- **Thread sanitizer does not run with the Pi 5's default kernel**
  (`ThreadSanitizer: unsupported VMA range ... Found 47`). Options: boot `kernel8.img`
  (add `kernel=kernel8.img` to `/boot/firmware/config.txt`), or run the queue tests in a
  regular x86 Linux environment. Not yet done.

## License

This project's code is released under the MIT License (see `LICENSE`). Third-party model weights
are not included in this repository and keep their own licenses (see Models).
