# Shared configuration for audio streaming
PACKET_SAMPLES = 128  # Must match in feeder, C engine, and GUI receiver
TCP_HOST = "127.0.0.1"
TCP_PORT = 54321
PIPE_NAME = r"\\.\pipe\ni6009"

# Decimation factor for socket visualization data.
# Must match SAMPLE_DECIMATION in audio_rpi/include/config.h.
# 1 = off (full resolution), 2 = 50% reduction, 4 = 75% reduction (recommended).
SAMPLE_DECIMATION = 4