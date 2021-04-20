/*
 * Copyright (c) 2015 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_sndio/sox_sink.h"
#include "roc_core/log.h"
#include "roc_core/panic.h"
#include "roc_sndio/sox_backend.h"

namespace roc {
namespace sndio {

SoxSink::SoxSink(core::IAllocator& allocator, const Config& config)
    : output_(NULL)
    , buffer_(allocator)
    , buffer_size_(0)
    , is_file_(false)
    , valid_(false) {
    SoxBackend::instance();

    if (config.sample_spec.num_channels() == 0) {
        roc_log(LogError, "sox sink: # of channels is zero");
        return;
    }

    if (config.latency != 0) {
        roc_log(LogError, "sox sink: setting io latency not supported by sox backend");
        return;
    }

    frame_length_ = config.frame_length;
    sample_spec_ = config.sample_spec;

    if (frame_length_ == 0) {
        roc_log(LogError, "sox sink: frame length is zero");
        return;
    }

    memset(&out_signal_, 0, sizeof(out_signal_));
    out_signal_.rate = config.sample_spec.sample_rate();
    out_signal_.channels = (unsigned)config.sample_spec.num_channels();
    out_signal_.precision = SOX_SAMPLE_PRECISION;

    valid_ = true;
}

SoxSink::~SoxSink() {
    close_();
}

bool SoxSink::valid() const {
    return valid_;
}

bool SoxSink::open(const char* driver, const char* output) {
    roc_panic_if(!valid_);

    roc_log(LogInfo, "sox sink: opening: driver=%s output=%s", driver, output);

    if (buffer_.size() != 0 || output_) {
        roc_panic("sox sink: can't call open() more than once");
    }

    if (!open_(driver, output)) {
        return false;
    }

    if (!setup_buffer_()) {
        return false;
    }

    return true;
}

size_t SoxSink::sample_rate() const {
    roc_panic_if(!valid_);

    if (!output_) {
        roc_panic("sox sink: sample_rate: non-open output file or device");
    }

    return size_t(output_->signal.rate);
}

size_t SoxSink::num_channels() const {
    roc_panic_if(!valid_);

    if (!output_) {
        roc_panic("sox sink: num_channels: non-open output file or device");
    }

    return size_t(output_->signal.channels);
}

bool SoxSink::has_clock() const {
    roc_panic_if(!valid_);

    if (!output_) {
        roc_panic("sox sink: has_clock: non-open output file or device");
    }

    return !is_file_;
}

void SoxSink::write(audio::Frame& frame) {
    roc_panic_if(!valid_);

    const audio::sample_t* frame_data = frame.data();
    size_t frame_size = frame.size();

    sox_sample_t* buffer_data = buffer_.data();
    size_t buffer_pos = 0;

    SOX_SAMPLE_LOCALS;

    size_t clips = 0;

    while (frame_size > 0) {
        for (; buffer_pos < buffer_size_ && frame_size > 0; buffer_pos++) {
            buffer_data[buffer_pos] = SOX_FLOAT_32BIT_TO_SAMPLE(*frame_data, clips);
            frame_data++;
            frame_size--;
        }

        if (buffer_pos == buffer_size_) {
            write_(buffer_data, buffer_pos);
            buffer_pos = 0;
        }
    }

    write_(buffer_data, buffer_pos);
}

bool SoxSink::setup_buffer_() {
    buffer_size_ = sample_spec_.ns_to_size(frame_length_);
    if (buffer_size_ == 0) {
        roc_log(LogError, "sox sink: buffer size is zero");
        return false;
    }
    if (!buffer_.resize(buffer_size_)) {
        roc_log(LogError, "sox sink: can't allocate sample buffer");
        return false;
    }

    return true;
}

bool SoxSink::open_(const char* driver, const char* output) {
    output_ = sox_open_write(output, &out_signal_, NULL, driver, NULL, NULL);
    if (!output_) {
        roc_log(LogError, "sox sink: can't open writer: driver=%s output=%s", driver,
                output);
        return false;
    }

    is_file_ = !(output_->handler.flags & SOX_FILE_DEVICE);

    unsigned long in_rate = (unsigned long)out_signal_.rate;
    unsigned long out_rate = (unsigned long)output_->signal.rate;

    if (in_rate != 0 && in_rate != out_rate) {
        roc_log(
            LogError,
            "sox sink: can't open output file or device with the required sample rate: "
            "required_by_output=%lu requested_by_user=%lu",
            out_rate, in_rate);
        return false;
    }
    
    sample_spec_.set_sample_rate((unsigned long)output_->signal.rate);

    roc_log(LogInfo,
            "sox sink:"
            " bits=%lu out_rate=%lu in_rate=%lu ch=%lu is_file=%d",
            (unsigned long)output_->encoding.bits_per_sample, out_rate, in_rate,
            (unsigned long)output_->signal.channels, (int)is_file_);

    return true;
}

void SoxSink::write_(const sox_sample_t* samples, size_t n_samples) {
    if (n_samples > 0) {
        if (sox_write(output_, samples, n_samples) != n_samples) {
            roc_log(LogError, "sox sink: failed to write output buffer");
        }
    }
}

void SoxSink::close_() {
    if (!output_) {
        return;
    }

    roc_log(LogInfo, "sox sink: closing output");

    int err = sox_close(output_);
    if (err != SOX_SUCCESS) {
        roc_panic("sox sink: can't close output: %s", sox_strerror(err));
    }

    output_ = NULL;
}

} // namespace sndio
} // namespace roc
