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
| 3 | Pretrained person detector (OpenCV DNN) | In progress |
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
stamp captured_at + sequence    --->   process the frame
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

## Models

| Model                               | Source 
|-------------------------------------|------------------------
| SSD MobileNet v2 (COCO, 2018-03-29) | TensorFlow model zoo (`frozen_inference_graph.pb`) + OpenCV 
                                        `opencv_extra` (`ssd_mobilenet_v2_coco_2018_03_29.pbtxt`) 

| License    | Input         | Classes used |
|------------|---------------|--------------|
| Apache 2.0 | 300 × 300 RGB | 1 = person   |

Model files are not stored in this repository. Download them into `models/`.

**Confidence threshold:** 0.3, provisional. For a safety monitor, a missed person is worse than
a false stop, so the threshold starts low. The final value will be chosen from the week 7
validation set against a stated missed-detection requirement, not by eye.

## Known issues

- **Thread sanitizer does not run with the Pi 5's default kernel**
  (`ThreadSanitizer: unsupported VMA range ... Found 47`). Options: boot `kernel8.img`
  (add `kernel=kernel8.img` to `/boot/firmware/config.txt`), or run the queue tests in a
  regular x86 Linux environment. Not yet done.

## License

TODO: choose a license for this repository. Third-party model weights keep their own licenses
(see the Models section, added in week 3).
