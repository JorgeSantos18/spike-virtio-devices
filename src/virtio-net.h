#include <sys/select.h>
#include <riscv/abstract_device.h>
#include <riscv/simif.h>
#include <riscv/abstract_interrupt_controller.h>
#include <riscv/mmu.h>
#include <riscv/processor.h>
#include <riscv/simif.h>
#include <riscv/sim.h>
#include <riscv/dts.h>
#include <fdt/libfdt.h>
#include "virtio.h"
#include "list.h" 
#include "slirp/libslirp.h"

#define VIRTIO_NET_BASE 0x50011000
#define VIRTIO_NET_IRQ       5

class virtionet_t: public virtio_base_t {
public:
  virtionet_t(
      const simif_t* sim,
      abstract_interrupt_controller_t *intctrl,
      uint32_t interrupt_id,
      std::vector<std::string> sargs);
  ~virtionet_t();
private:
};