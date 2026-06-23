#include "drivers/video/gpu.h"
#include "drivers/bus/pci.h"
#include "kernel/mem/vmm.h"

extern gpu_driver_t intel_gpu_driver;
extern gpu_driver_t virtio_gpu_driver;

static gpu_device_t primary_gpu;
static int has_gpu = 0;

extern uint64_t hhdm_offset;

#define GPU_MMIO_VIRT    0xFFFF900000000000
#define GPU_FB_VIRT      0xFFFFA00000000000

extern void serial_puts(const char* s);

static uint64_t get_pci_bar_addr(pci_device_t* pci, int bar_idx) {
    uint32_t bar_val = pci->bar[bar_idx];
    if ((bar_val & 0x6) == 0x04) { // 64-bit BAR
        return (uint64_t)(bar_val & ~0xF) | ((uint64_t)pci->bar[bar_idx + 1] << 32);
    }
    return (uint64_t)(bar_val & ~0xF);
}

void gpu_init(void) {
    serial_puts("[GPU] Initializing...\n");
    int pci_count = pci_get_device_count();
    for (int i = 0; i < pci_count; i++) {
        pci_device_t* pci = pci_get_device(i);
        
        // 1. Check for Intel GPU
        if (pci->vendor_id == 0x8086 && pci->class_id == 0x03) {
            serial_puts("[GPU] Found Intel GPU\n");
            
            primary_gpu.vendor_id = pci->vendor_id;
            primary_gpu.device_id = pci->device_id;
            
            uint64_t mmio_phys = get_pci_bar_addr(pci, 0);
            uint64_t fb_phys = get_pci_bar_addr(pci, 2);
            
            primary_gpu.mmio_size = 16 * 1024 * 1024; // Gen9 default
            
            serial_puts("[GPU] Mapping MMIO...\n");
            // Map MMIO and Aperture
            vmm_map_range(GPU_MMIO_VIRT, mmio_phys, primary_gpu.mmio_size, VMM_WRITE | VMM_PCD);
            serial_puts("[GPU] Mapping FB...\n");
            vmm_map_range(GPU_FB_VIRT, fb_phys, 256 * 1024 * 1024, VMM_WRITE | VMM_PCD);
            serial_puts("[GPU] Mapped BARs\n");

            primary_gpu.mmio_base = GPU_MMIO_VIRT;
            primary_gpu.fb_base = GPU_FB_VIRT;
            primary_gpu.phys_fb_base = fb_phys;
            
            extern uint32_t get_fb_width();
            extern uint32_t get_fb_height();
            extern uint32_t gfx_get_stride();
            
            primary_gpu.width = get_fb_width();
            primary_gpu.height = get_fb_height();
            primary_gpu.pitch = gfx_get_stride() * 4; // Assuming 32bpp

            primary_gpu.initialized = 0;
            primary_gpu.driver = &intel_gpu_driver;
            
            const char* name = "Intel Graphics (Compatible Mode)";
            if (pci->device_id == INTEL_HD_620 || pci->device_id == INTEL_HD_620_ALT) 
                name = "Intel HD Graphics 620 (Compatible)";
            else if (pci->device_id == INTEL_HD_520 || pci->device_id == INTEL_HD_520_ALT || pci->device_id == INTEL_HD_520_MIN)
                name = "Intel HD Graphics 520 (Compatible)";

            int k = 0;
            while (name[k]) { primary_gpu.name[k] = name[k]; k++; }
            primary_gpu.name[k] = 0;

            if (primary_gpu.driver->init(&primary_gpu) == 0) {
                has_gpu = 1;
                return;
            }
        }

        // 2. Check for VirtIO GPU (QEMU)
        if (pci->vendor_id == 0x1AF4 && (pci->device_id == 0x1010 || pci->device_id == 0x1050 || pci->device_id == 0x1009)) {
            serial_puts("[GPU] Found VirtIO GPU\n");
            primary_gpu.vendor_id = pci->vendor_id;
            primary_gpu.device_id = pci->device_id;

            uint64_t mmio_phys = get_pci_bar_addr(pci, 4); // BAR4: Common Config / Notify
            uint64_t fb_phys = get_pci_bar_addr(pci, 2);   // BAR2: Framebuffer

            // Map VirtIO BARs
            vmm_map_range(GPU_MMIO_VIRT, mmio_phys, 16 * 1024 * 1024, VMM_WRITE | VMM_PCD);
            vmm_map_range(GPU_FB_VIRT, fb_phys, 256 * 1024 * 1024, VMM_WRITE | VMM_PCD);

            primary_gpu.mmio_base = GPU_MMIO_VIRT;
            primary_gpu.fb_base = GPU_FB_VIRT;
            primary_gpu.phys_fb_base = fb_phys;

            extern uint32_t get_fb_width();
            extern uint32_t get_fb_height();
            extern uint32_t gfx_get_stride();
            
            primary_gpu.width = get_fb_width();
            primary_gpu.height = get_fb_height();
            primary_gpu.pitch = gfx_get_stride() * 4;

            primary_gpu.initialized = 0;
            primary_gpu.driver = &virtio_gpu_driver;

            const char* name = "VirtIO GPU (Compatible Mode)";
            int k = 0;
            while (name[k]) { primary_gpu.name[k] = name[k]; k++; }
            primary_gpu.name[k] = 0;
            
            if (primary_gpu.driver->init(&primary_gpu) == 0) {
                has_gpu = 1;
                return;
            }
        }

        // 3. Check for QEMU VGA
        if (pci->vendor_id == 0x1234 && pci->device_id == 0x1111) {
            primary_gpu.vendor_id = pci->vendor_id;
            primary_gpu.device_id = pci->device_id;
            primary_gpu.initialized = 0;
            primary_gpu.driver = &virtio_gpu_driver;

            const char* name = "Standard VGA (Compatible Mode)";
            int k = 0;
            while (name[k]) { primary_gpu.name[k] = name[k]; k++; }
            primary_gpu.name[k] = 0;
            
            if (primary_gpu.driver->init(&primary_gpu) == 0) {
                has_gpu = 1;
                return;
            }
        }
    }
}

gpu_device_t* gpu_get_primary(void) { return has_gpu ? &primary_gpu : NULL; }

int gpu_accel_fill(int x, int y, int w, int h, uint32_t color) {
    if (!has_gpu || !primary_gpu.driver || !primary_gpu.driver->accel_2d) return -1;
    gpu_2d_params_t p = { .op = GPU_OP_FILL, .dst_x = x, .dst_y = y, .width = w, .height = h, .color = color };
    return primary_gpu.driver->accel_2d(&primary_gpu, &p);
}

int gpu_accel_blend(void* src, int dx, int dy, int w, int h, int alpha) {
    if (!has_gpu || !primary_gpu.driver || !primary_gpu.driver->accel_2d) return -1;
    gpu_2d_params_t p = { .op = GPU_OP_BLEND, .dst_x = dx, .dst_y = dy, .width = w, .height = h, .src_ptr = src, .alpha = alpha };
    return primary_gpu.driver->accel_2d(&primary_gpu, &p);
}

uint32_t gpu_get_capabilities(void) {
    if (!has_gpu || !primary_gpu.driver || !primary_gpu.driver->get_caps) return 0;
    return primary_gpu.driver->get_caps(&primary_gpu);
}

int gpu_accel_3d_test(void) {
    if (!has_gpu || !primary_gpu.driver || !primary_gpu.driver->accel_3d) return -1;
    return primary_gpu.driver->accel_3d(&primary_gpu, NULL, 0);
}
