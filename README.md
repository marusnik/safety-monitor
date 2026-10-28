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
| 4 | Distance sensor (VL53L1X over I2C), zone logic with hysteresis | Planned |
| 5 | ML safety monitor: confidence intervals, plausibility checks, fail-safe STOP | Planned |
| 6 | Cross-checking the camera against the distance sensor | Planned |
| 7 | Rare-scenario validation set (low light, occlusion, ...) | Planned |
| 8 | Profiling, sanitizers, CI | Planned |
| 9 | Requirements traced to tests, safety argument | Planned |
| 10 | Documentation and demo | Planned |

## Hardware

- Raspberry Pi 5 (8 GB), active cooler, 27 W USB-C power supply
- Microsoft LifeCam HD-5001 USB webcam (UVC, appears as `/dev/video0`)
- Planned: VL53L1X time-of-flight distance sensor (I2C)

## Software

- Raspberry Pi OS (64-bit)
- C++20, g++ 14, CMake 3.31
- OpenCV 4.10 (`libopencv-dev`)

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
./build/safety_monitor            # runs the capture pipeline for 10 s and prints a report
./build/detect_image frame.jpg    # runs person detection on one image, saves result.jpg
./build/test_frame_queue          # ring buffer tests
./build/test_thread_safe_queue    # thread-safe queue tests (two threads)
```

No output from a test program means all tests passed.

## Project structure

```
include/
  frame.hpp               Frame: image + capture timestamp + sequence number
  frame_queue.hpp         RingBuffer<T, Capacity>: fixed-size, no heap, overwrite-oldest policy
  thread_safe_queue.hpp   ThreadSafeQueue<T, Capacity>: mutex + condition variable around RingBuffer
  person_detector.hpp     PersonDetector: loads the model, returns person boxes above a threshold
src/
  main.cpp                camera thread, processing thread, report
  person_detector.cpp
  detect_image.cpp        command-line tool: detect people in one image
models/                   model files (not in git, see Models)
tests/
  test_frame_queue.cpp
  test_thread_safe_queue.cpp
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

- **Thread sanitizer does not run with the Pi 5's default kernel**
  (`ThreadSanitizer: unsupported VMA range ... Found 47`). Options: boot `kernel8.img`
  (add `kernel=kernel8.img` to `/boot/firmware/config.txt`), or run the queue tests in a
  regular x86 Linux environment. Not yet done.

## License

This project's code is released under the MIT License (see `LICENSE`). Third-party model weights
are not included in this repository and keep their own licenses (see Models).
