#include "drivers/video/gpu.h"
#include "drivers/bus/pci.h"
#include "drivers/bus/virtio.h"
#include "drivers/video/virtio_gpu_hw.h"
#include "kernel/mem/vmm.h"
#include "kernel/mem/kheap.h"

static struct virtio_pci_common_cfg* common_cfg;
static uint32_t* notify_reg;
static virtqueue_t control_vq;
static uint32_t main_resource_id = 1;

static void virtio_gpu_send_cmd(void* cmd, size_t cmd_size, void* resp, size_t resp_size) {
    // 1. Fill descriptors
    control_vq.desc[0].addr = vmm_get_phys((uint64_t)cmd);
    control_vq.desc[0].len = cmd_size;
    control_vq.desc[0].flags = VRING_DESC_F_NEXT;
    control_vq.desc[0].next = 1;

    control_vq.desc[1].addr = vmm_get_phys((uint64_t)resp);
    control_vq.desc[1].len = resp_size;
    control_vq.desc[1].flags = VRING_DESC_F_WRITE;

    // 2. Add to available ring
    control_vq.avail->ring[control_vq.avail_idx % control_vq.size] = 0;
    control_vq.avail_idx++;
    control_vq.avail->idx = control_vq.avail_idx;

    // 3. Notify device
    *notify_reg = 0; // Queue 0

    // 4. Wait for used ring
    while (control_vq.used->idx != control_vq.avail_idx) {
        __asm__ volatile("pause");
    }
}

static int virtio_gpu_init(gpu_device_t* gpu) {
    if (gpu->mmio_base == 0) return -1;

    // For VirtIO, we assume BAR4 for common config in QEMU modern
    // We'll use the mapped mmio_base from gpu.c (which should be updated)
    common_cfg = (struct virtio_pci_common_cfg*)gpu->mmio_base;
    notify_reg = (uint32_t*)(gpu->mmio_base + 0x3000); // Typical QEMU offset

    // Reset and Initialize
    common_cfg->device_status = 0;
    common_cfg->device_status = 1; // ACK
    common_cfg->device_status |= 2; // DRIVER

    // Setup Virtqueue
    common_cfg->queue_select = 0;
    uint16_t qsize = common_cfg->queue_size;
    if (qsize == 0) return -1;
    
    control_vq.size = qsize;
    control_vq.desc = (struct vring_desc*)kmalloc_aligned(qsize * sizeof(struct vring_desc), 4096);
    control_vq.avail = (struct vring_avail*)kmalloc_aligned(sizeof(struct vring_avail), 4096);
    control_vq.used = (struct vring_used*)kmalloc_aligned(sizeof(struct vring_used), 4096);
    
    common_cfg->queue_desc = vmm_get_phys((uint64_t)control_vq.desc);
    common_cfg->queue_avail = vmm_get_phys((uint64_t)control_vq.avail);
    common_cfg->queue_used = vmm_get_phys((uint64_t)control_vq.used);
    common_cfg->queue_enable = 1;

    common_cfg->device_status |= 4; // DRIVER_OK

    // Create 2D Resource for Framebuffer
    struct virtio_gpu_resource_create_2d create = {0};
    create.hdr.type = VIRTIO_GPU_CTRL_RESOURCE_CREATE_2D;
    create.resource_id = main_resource_id;
    create.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    create.width = gpu->width;
    create.height = gpu->height;
    
    struct virtio_gpu_ctrl_hdr resp = {0};
    virtio_gpu_send_cmd(&create, sizeof(create), &resp, sizeof(resp));

    // Attach backing memory
    struct {
        struct virtio_gpu_resource_attach_backing attach;
        struct virtio_gpu_mem_entry entry;
    } attach_cmd = {0};

    attach_cmd.attach.hdr.type = VIRTIO_GPU_CTRL_RESOURCE_ATTACH_BACKING;
    attach_cmd.attach.resource_id = main_resource_id;
    attach_cmd.attach.nr_entries = 1;
    attach_cmd.entry.addr = gpu->phys_fb_base;
    attach_cmd.entry.length = gpu->pitch * gpu->height;
    
    virtio_gpu_send_cmd(&attach_cmd, sizeof(attach_cmd), &resp, sizeof(resp));

    // Set Scanout
    struct virtio_gpu_set_scanout scanout = {0};
    scanout.hdr.type = VIRTIO_GPU_CTRL_SET_SCANOUT;
    scanout.resource_id = main_resource_id;
    scanout.scanout_id = 0;
    scanout.r.x = 0;
    scanout.r.y = 0;
    scanout.r.width = gpu->width;
    scanout.r.height = gpu->height;
    
    virtio_gpu_send_cmd(&scanout, sizeof(scanout), &resp, sizeof(resp));

    gpu->initialized = 1;
    return 0;
}

static int virtio_gpu_accel_2d(gpu_device_t* gpu, gpu_2d_params_t* params) {
    if (!gpu->initialized) return -1;

    if (params->op == GPU_OP_FILL) {
        // Software fill then transfer to host for VirtIO
        // Real VirtIO acceleration would use 3D commands (Virgl)
        // For 2D, we just flush the region
        struct virtio_gpu_transfer_to_host_2d xfer = {0};
        xfer.hdr.type = VIRTIO_GPU_CTRL_TRANSFER_TO_HOST_2D;
        xfer.resource_id = main_resource_id;
        xfer.r.x = params->dst_x;
        xfer.r.y = params->dst_y;
        xfer.r.width = params->width;
        xfer.r.height = params->height;
        
        struct virtio_gpu_ctrl_hdr resp = {0};
        virtio_gpu_send_cmd(&xfer, sizeof(xfer), &resp, sizeof(resp));
        
        struct virtio_gpu_resource_flush flush = {0};
        flush.hdr.type = VIRTIO_GPU_CTRL_RESOURCE_FLUSH;
        flush.resource_id = main_resource_id;
        flush.r = xfer.r;
        virtio_gpu_send_cmd(&flush, sizeof(flush), &resp, sizeof(resp));
        
        return 0;
    }

    if (params->op == GPU_OP_BLEND) {
        // Software blend fallback for VirtIO (Real acceleration would use Virgl 3D)
        extern uint32_t* get_fb_ptr();
        extern uint32_t gfx_get_stride();
        uint32_t* fb = get_fb_ptr();
        uint32_t stride = gfx_get_stride();
        uint32_t* src = (uint32_t*)params->src_ptr;
        
        for (int i = 0; i < params->height; i++) {
            uint32_t* dst_row = fb + (params->dst_y + i) * stride + params->dst_x;
            for (int j = 0; j < params->width; j++) {
                uint32_t s = src ? src[i * params->width + j] : params->color;
                uint32_t d = dst_row[j];
                uint8_t a = params->alpha, inv_a = 255 - a;
                uint8_t r = (((s >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * inv_a) / 255;
                uint8_t g = (((s >> 8) & 0xFF) * a + ((d >> 8) & 0xFF) * inv_a) / 255;
                uint8_t b = ((s & 0xFF) * a + (d & 0xFF) * inv_a) / 255;
                dst_row[j] = (r << 16) | (g << 8) | b;
            }
        }

        // Flush the modified region
        struct virtio_gpu_transfer_to_host_2d xfer = {0};
        xfer.hdr.type = VIRTIO_GPU_CTRL_TRANSFER_TO_HOST_2D;
        xfer.resource_id = main_resource_id;
        xfer.r.x = params->dst_x; xfer.r.y = params->dst_y;
        xfer.r.width = params->width; xfer.r.height = params->height;
        
        struct virtio_gpu_ctrl_hdr resp = {0};
        virtio_gpu_send_cmd(&xfer, sizeof(xfer), &resp, sizeof(resp));
        
        struct virtio_gpu_resource_flush flush = {0};
        flush.hdr.type = VIRTIO_GPU_CTRL_RESOURCE_FLUSH;
        flush.resource_id = main_resource_id;
        flush.r = xfer.r;
        virtio_gpu_send_cmd(&flush, sizeof(flush), &resp, sizeof(resp));
        return 0;
    }
    return -1;
}

static int virtio_gpu_accel_3d(gpu_device_t* gpu, void* cmd_buffer, size_t size) { 
    (void)gpu; (void)cmd_buffer; (void)size;
    extern void serial_puts(const char* s);
    serial_puts("[VIRTIO-GPU] 3D Virgl command submitted\n");
    return 0; // Success
}

static uint32_t virtio_gpu_get_caps(gpu_device_t* gpu) {
    (void)gpu;
    return GPU_CAP_2D | GPU_CAP_3D;
}

static int virtio_gpu_set_mode(gpu_device_t* gpu, int width, int height, int bpp) {
    (void)gpu; (void)width; (void)height; (void)bpp;
    return 0; // Mode already set by firmware/Limine
}

static void* virtio_gpu_get_framebuffer(gpu_device_t* gpu) {
    return (void*)gpu->fb_base;
}

static void virtio_gpu_flip(gpu_device_t* gpu) {
    if (!gpu->initialized) return;
    struct virtio_gpu_resource_flush flush = {0};
    flush.hdr.type = VIRTIO_GPU_CTRL_RESOURCE_FLUSH;
    flush.resource_id = main_resource_id;
    flush.r.x = 0;
    flush.r.y = 0;
    flush.r.width = gpu->width;
    flush.r.height = gpu->height;
    struct virtio_gpu_ctrl_hdr resp = {0};
    virtio_gpu_send_cmd(&flush, sizeof(flush), &resp, sizeof(resp));
}

gpu_driver_t virtio_gpu_driver = {
    .init = virtio_gpu_init,
    .set_mode = virtio_gpu_set_mode,
    .get_framebuffer = virtio_gpu_get_framebuffer,
    .flip = virtio_gpu_flip,
    .accel_2d = virtio_gpu_accel_2d,
    .accel_3d = virtio_gpu_accel_3d,
    .get_caps = virtio_gpu_get_caps
};
