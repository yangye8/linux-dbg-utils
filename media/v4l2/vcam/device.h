#ifndef VCAM_DEVICE_H
#define VCAM_DEVICE_H

#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>

#include "vcam.h"

#define PIXFMTS_MAX 4
#define FB_NAME_MAXLENGTH 16

struct vcam_in_buffer {
    void *data;
    size_t filled;
    uint32_t jiffies;
};

struct vcam_in_queue {
    struct vcam_in_buffer buffers[2];
    struct vcam_in_buffer dummy;
    struct vcam_in_buffer *pending;
    struct vcam_in_buffer *ready;
};

struct vcam_out_buffer {
    struct vb2_buffer vb;
    struct list_head list;
    size_t filled;
};

struct vcam_out_queue {
    struct list_head active;
    int frame;
    /* TODO: implement more */
};

struct vcam_device_format {
    char *name;
    int fourcc;
    int bit_depth;
};

struct vcam_device {
    dev_t dev_number;
    struct v4l2_device v4l2_dev;
    struct video_device vdev;

    /* input buffer */
    struct vcam_in_queue in_queue;
    spinlock_t in_q_slock;
    spinlock_t in_fh_slock;
    bool fb_isopen;

    /* output buffer */
    struct vb2_queue vb_out_vidq;
    struct vcam_out_queue vcam_out_vidq;
    spinlock_t out_q_slock;
    /* Output framerate */
    struct v4l2_fract output_fps;

    /* Input framebuffer */
    char vcam_fb_fname[FB_NAME_MAXLENGTH];
    struct proc_dir_entry *vcam_fb_procf;
    struct mutex vcam_mutex;

    /* Submitter thread */
    struct task_struct *sub_thr_id;

    /* Format descriptor */
    size_t nr_fmts;
    struct vcam_device_format out_fmts[PIXFMTS_MAX];

    struct v4l2_pix_format output_format;
    struct v4l2_pix_format input_format;

    /* Conversion switches */
    bool conv_pixfmt_on;
    bool conv_res_on;
};

struct vcam_device *create_vcam_device(size_t idx,
                                       struct vcam_device_spec *dev_spec);
void destroy_vcam_device(struct vcam_device *vcam);

int submitter_thread(void *data);

#endif
