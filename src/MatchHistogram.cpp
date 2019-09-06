
// MatchHistogram(clip c1, clip c2, int plane, bool raw, bool show, bool debug)
// ----------------------------------------------------------------------------
// Try to modify clip c1 histogram to match that of c2.
// Should be used for analysis only, not for production.
// Clips must be pixel aligned to produce coherent result.
// Only planar color spaces are supported.
// ----------------------------------------------------------------------------
// plane [default=0]: 0=Y, 1=U, 2=V, 3=U+V, 4=Y+U+V
// raw   [default=false]: use raw histogram without postprocessing
// show  [default=false]: show calculated curve on video frame
// debug [default=false]: return 256x256 YV12 clip  with calculated data
// ----------------------------------------------------------------------------
// Created by LaTo INV. for forum.doom9.org


#include <algorithm>
#include <cstdint>
#include <cstring>

#include <VapourSynth.h>
#include <VSHelper.h>


static inline int IntDiv(int x, int y) {
    return ((x < 0) ^ (y < 0)) ? ((x - (y >> 1)) / y)
                               : ((x + (y >> 1)) / y);
}


static inline void fillPlane(uint8_t *data, int width, int height, int stride, int value) {
    for (int y = 0; y < height; y++) {
        memset(data, value, width);

        data += stride;
    }
}


class CurveData {
private:
    unsigned int sum[256];
    unsigned int div[256];
    unsigned char curve[256];

public:
    void Create(const uint8_t *ptr1, const uint8_t *ptr2, int width, int height, int stride, bool raw, int smoothing_window) {
        // Clear data
        for (int i = 0; i < 256; i++) {
            sum[i] = 0;
            div[i] = 0;
        }

        // Populate data
        for (int h = 0; h < height; h++) {
            for (int w = 0; w < width; w++) {
                sum[ptr1[w]] += ptr2[w];
                div[ptr1[w]] += 1;
            }
            ptr1 += stride;
            ptr2 += stride;
        }

        // Raw curve
        for (int i = 0; i < 256; i++) {
            if (div[i] != 0) {
                curve[i] = IntDiv(sum[i], div[i]);
            } else {
                curve[i] = 0;
            }
        }

        if (!raw) {
            int flat = -1;
            for (int i = 0; i < 256; i++) {
                if (div[i] != 0) {
                    if (flat == -1) {
                        flat = i;
                    } else {
                        flat = -1;
                        break;
                    }
                }
            }

            if (flat != -1) {
                // Uniform color
                for (int i = 0; i < 256; i++) {
                    curve[i] = curve[flat];
                }
            } else {
                for (int i = 0; i < 256; i++) {
                    if (div[i] == 0) {
                        int prev = -1;
                        for (int p = i - 1; p >= 0; p--) {
                            if (div[p] != 0) {
                                prev = p;
                                break;
                            }
                        }

                        int next = -1;
                        for (int n = i + 1; n < 256; n++) {
                            if (div[n] != 0) {
                                next = n;
                                break;
                            }
                        }

                        // Fill missing
                        if (prev != -1 && next != -1) {
                            curve[i] = std::min(std::max(curve[prev] + IntDiv((i - prev) * (curve[next] - curve[prev]), (next - prev)), 0), 255);
                            sum[i] = curve[i];
                            div[i] = 1;
                        }
                    }
                }

                while (div[0] == 0 || div[255] == 0) {
                    if (div[0] == 0) {
                        int first = -1;
                        for (int f = 0; f < 256; f++) {
                            if (div[f] != 0) {
                                first = f;
                                break;
                            }
                        }

                        // Extend bottom
                        for (int i = 0; i < first; i++) {
                            if (first * 2 - i <= 255) {
                                if (div[first * 2 - i] != 0) {
                                    curve[i] = std::min(std::max(curve[first] * 2 - curve[first * 2 - i], 0), 255);
                                    sum[i] = curve[i];
                                    div[i] = 1;
                                }
                            }
                        }
                    }

                    if (div[255] == 0) {
                        int last = -1;
                        for (int l = 255; l >= 0; l--) {
                            if (div[l] != 0) {
                                last = l;
                                break;
                            }
                        }

                        // Extend top
                        for (int i = 255; i > last; i--) {
                            if (last * 2 - i >= 0) {
                                if (div[last * 2 - i] != 0) {
                                    curve[i] = std::min(std::max(curve[last] * 2 - curve[last * 2 - i], 0), 255);
                                    sum[i] = curve[i];
                                    div[i] = 1;
                                }
                            }
                        }
                    }
                }

                // Smooth curve
                if (smoothing_window > 0) {
                    for (int i = 0; i < 256; i++) {
                        sum[i] = 0;
                        div[i] = 0;

                        for (int j = -smoothing_window; j < +smoothing_window; j++) {
                            if (i + j >= 0 && i + j < 256) {
                                sum[i] += curve[i + j];
                                div[i] += 1;
                            }
                        }
                    }
                }

                for (int i = 0; i < 256; i++) {
                    curve[i] = IntDiv(sum[i], div[i]);
                }
            }
        }
    }

    void Process(uint8_t *ptr, int width, int height, int stride) {
        for (int h = 0; h < height; h++) {
            for (int w = 0; w < width; w++)
                ptr[w] = curve[ptr[w]];

            ptr += stride;
        }
    }

    void Show(uint8_t *ptr, int stride, uint8_t color) {
        for (int i = 0; i < 256; i++)
            ptr[((255 - curve[i]) * stride) + i] = color;
    }

    void Debug(uint8_t *ptr, int stride) {
        for (int i = 0; i < 256; i++) {
            for (int j = 0; j <= curve[i]; j++) {
                ptr[((255 - j) * stride) + i] = curve[i];
            }
        }

        for (int i = 0; i < 256; i++) {
            if (curve[i] > 0) {
                ptr[((255 - curve[i]) * stride) + i] = 255;
            }
        }
    }
};


struct MatchHistogramData {
    VSNodeRef *clip1;
    VSNodeRef *clip2;
    bool raw;
    bool show;
    bool debug;
    int smoothing_window;
    int process[3];
    VSVideoInfo vi;
};


static void VS_CC MatchHistogramInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    (void)in;
    (void)out;
    (void)core;

    MatchHistogramData *d = (MatchHistogramData *) *instanceData;

    vsapi->setVideoInfo(&d->vi, 1, node);
}


static const VSFrameRef *VS_CC MatchHistogramGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    const MatchHistogramData *d = (const MatchHistogramData *) *instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->clip1, frameCtx);
        vsapi->requestFrameFilter(n, d->clip2, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src1 = vsapi->getFrameFilter(n, d->clip1, frameCtx);
        const VSFrameRef *src2 = vsapi->getFrameFilter(n, d->clip2, frameCtx);

        VSFrameRef *dst;

        CurveData curve;

        if (d->debug) {
            dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src1, core);

            for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
                uint8_t *dstp = vsapi->getWritePtr(dst, plane);
                int dst_width = vsapi->getFrameWidth(dst, plane);
                int dst_height = vsapi->getFrameHeight(dst, plane);
                int dst_stride = vsapi->getStride(dst, plane);

                fillPlane(dstp, dst_width, dst_height, dst_stride, plane ? 128 : 0);

                if (!d->process[plane])
                    continue;

                const uint8_t *src1p = vsapi->getReadPtr(src1, plane);
                const uint8_t *src2p = vsapi->getReadPtr(src2, plane);
                int src_width = vsapi->getFrameWidth(src1, plane);
                int src_height = vsapi->getFrameHeight(src1, plane);
                int src_stride = vsapi->getStride(src1, plane);

                curve.Create(src1p, src2p, src_width, src_height, src_stride, d->raw, d->smoothing_window);
                curve.Debug(vsapi->getWritePtr(dst, 0),
                            vsapi->getStride(dst, 0));
            }
        } else { // Not debug
            const VSFrameRef *plane_src[3] = {
                d->process[0] ? nullptr : src1,
                d->process[1] ? nullptr : src1,
                d->process[2] ? nullptr : src1
            };

            int planes[3] = { 0, 1, 2 };

            dst = vsapi->newVideoFrame2(d->vi.format, d->vi.width, d->vi.height, plane_src, planes, src1, core);

            uint8_t show_colors[3] = { 235, 160, 96 };

            for (int plane = 0; plane < d->vi.format->numPlanes; plane++) {
                uint8_t *dstp = vsapi->getWritePtr(dst, plane);
                int stride = vsapi->getStride(dst, plane);

                if (d->process[plane]) {
                    const uint8_t *src1p = vsapi->getReadPtr(src1, plane);
                    const uint8_t *src2p = vsapi->getReadPtr(src2, plane);
                    int width = vsapi->getFrameWidth(src1, plane);
                    int height = vsapi->getFrameHeight(src1, plane);

                    curve.Create(src1p, src2p, width, height, stride, d->raw, d->smoothing_window);
                    curve.Process(dstp, width, height, stride);
                }

                if (d->show) {
                    fillPlane(dstp,
                              256 >> (plane ? d->vi.format->subSamplingW : 0),
                              256 >> (plane ? d->vi.format->subSamplingH : 0),
                              stride,
                              plane ? 128 : 16);

                    if (d->process[plane]) {
                        curve.Show(vsapi->getWritePtr(dst, 0),
                                   vsapi->getStride(dst, 0),
                                   show_colors[plane]);
                    }
                }
            }
        }

        vsapi->freeFrame(src1);
        vsapi->freeFrame(src2);

        return dst;
    }

    return nullptr;
}


static void VS_CC MatchHistogramFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    MatchHistogramData *d = (MatchHistogramData *)instanceData;

    vsapi->freeNode(d->clip1);
    vsapi->freeNode(d->clip2);
    free(d);
}


static void VS_CC MatchHistogramCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    (void)userData;

    MatchHistogramData d;
    memset(&d, 0, sizeof(d));

    int err;

    d.raw = !!vsapi->propGetInt(in, "raw", 0, &err);
    if (err)
        d.raw = false;

    d.show = !!vsapi->propGetInt(in, "show", 0, &err);
    if (err)
        d.show = false;

    d.debug = !!vsapi->propGetInt(in, "debug", 0, &err);
    if (err)
        d.debug = false;

    d.smoothing_window = int64ToIntS(vsapi->propGetInt(in, "smoothing_window", 0, &err));
    if (err)
        d.smoothing_window = 8;


    if (d.smoothing_window < 0) {
        vsapi->setError(out, "MatchHistogram: smoothing_window must not be negative.");
        return;
    }


    d.clip1 = vsapi->propGetNode(in, "clip1", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.clip1);

    d.clip2 = vsapi->propGetNode(in, "clip2", 0, nullptr);
    const VSVideoInfo *vi2 = vsapi->getVideoInfo(d.clip2);

    if (d.vi.format != vi2->format ||
        d.vi.width != vi2->width ||
        d.vi.height != vi2->height) {
        vsapi->setError(out, "MatchHistogram: the clips must have the same format and dimensions.");
        vsapi->freeNode(d.clip1);
        vsapi->freeNode(d.clip2);
        return;
    }

    if (!d.vi.format || d.vi.width == 0 || d.vi.height == 0) {
        vsapi->setError(out, "MatchHistogram: the clips must have constant format and dimensions.");
        vsapi->freeNode(d.clip1);
        vsapi->freeNode(d.clip2);
        return;
    }

    if (d.vi.format->colorFamily == cmRGB || d.vi.format->bitsPerSample > 8) {
        vsapi->setError(out, "MatchHistogram: the clips must have 8 bits per sample and must not be RGB.");
        vsapi->freeNode(d.clip1);
        vsapi->freeNode(d.clip2);
        return;
    }


    int n = d.vi.format->numPlanes;
    int m = vsapi->propNumElements(in, "planes");

    // By default only process the first plane
    if (m <= 0)
        d.process[0] = 1;

    for (int i = 0; i < m; i++) {
        int o = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (o < 0 || o >= n) {
            vsapi->freeNode(d.clip1);
            vsapi->freeNode(d.clip2);
            vsapi->setError(out, "MatchHistogram: plane index out of range");
            return;
        }

        if (d.process[o]) {
            vsapi->freeNode(d.clip1);
            vsapi->freeNode(d.clip2);
            vsapi->setError(out, "MatchHistogram: plane specified twice");
            return;
        }

        d.process[o] = 1;
    }

    if (d.debug) {
        if (d.process[0] + d.process[1] + d.process[2] > 1) {
            vsapi->setError(out, "MatchHistogram: only one plane can be processed at a time when debug is True.");
            vsapi->freeNode(d.clip1);
            vsapi->freeNode(d.clip2);
            return;
        }

        d.vi.width = 256;
        d.vi.height = 256;
    }

    if (d.show && (d.vi.width < 256 || d.vi.height < 256)) {
        vsapi->setError(out, "MatchHistogram: clips must be at least 256x256 pixels when show is True.");
        vsapi->freeNode(d.clip1);
        vsapi->freeNode(d.clip2);
        return;
    }


    MatchHistogramData *data = (MatchHistogramData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "MatchHistogram", MatchHistogramInit, MatchHistogramGetFrame, MatchHistogramFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.matchhistogram", "matchhist", "MatchHistogram", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("MatchHistogram",
                 "clip1:clip;"
                 "clip2:clip;"
                 "raw:int:opt;"
                 "show:int:opt;"
                 "debug:int:opt;"
                 "smoothing_window:int:opt;"
                 "planes:int[]:opt;"
                 , MatchHistogramCreate, nullptr, plugin);
}
