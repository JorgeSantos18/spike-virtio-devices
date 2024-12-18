#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <stdarg.h>
#include "virtio-net.h"
#include "cutils.h"

/*******************************************************/
/* slirp */
#ifdef CONFIG_SLIRP
static Slirp *slirp_state;

static void slirp_write_packet(EthernetDevice *net,
                               const uint8_t *buf, int len)
{
    Slirp *slirp_state = (Slirp *) net->opaque;
    slirp_input(slirp_state, buf, len);
}

int slirp_can_output(void *opaque)
{
    EthernetDevice *net = (EthernetDevice *) opaque;
    return net->device_can_write_packet(net);
}

void slirp_output(void *opaque, const uint8_t *pkt, int pkt_len)
{
    EthernetDevice *net = (EthernetDevice *) opaque;
    return net->device_write_packet(net, pkt, pkt_len);
}

static void slirp_select_fill1(EthernetDevice *net, int *pfd_max,
                               fd_set *rfds, fd_set *wfds, fd_set *efds,
                               int *pdelay)
{
    Slirp *slirp_state = (Slirp *) net->opaque;
    slirp_select_fill(slirp_state, pfd_max, rfds, wfds, efds);
}

static void slirp_select_poll1(EthernetDevice *net, 
                               fd_set *rfds, fd_set *wfds, fd_set *efds,
                               int select_ret)
{
    Slirp *slirp_state =  (Slirp *) net->opaque;
    slirp_select_poll(slirp_state, rfds, wfds, efds, (select_ret <= 0));
}

static EthernetDevice *slirp_open(void)
{
    EthernetDevice *net;
    struct in_addr net_addr  = { .s_addr = htonl(0x0a000200) }; /* 10.0.2.0 */
    struct in_addr mask = { .s_addr = htonl(0xffffff00) }; /* 255.255.255.0 */
    struct in_addr host = { .s_addr = htonl(0x0a000202) }; /* 10.0.2.2 */
    struct in_addr dhcp = { .s_addr = htonl(0x0a00020f) }; /* 10.0.2.15 */
    struct in_addr dns  = { .s_addr = htonl(0x0a000203) }; /* 10.0.2.3 */
    const char *bootfile = NULL;
    const char *vhostname = NULL;
    int restricted = 0;
    
    if (slirp_state) {
        fprintf(stderr, "Only a single slirp instance is allowed\n");
        return NULL;
    }
    net = (EthernetDevice *) mallocz(sizeof(*net));

    slirp_state = slirp_init(restricted, net_addr, mask, host, vhostname,
                             "", bootfile, dhcp, dns, net);
    
    net->mac_addr[0] = 0x02;
    net->mac_addr[1] = 0x00;
    net->mac_addr[2] = 0x00;
    net->mac_addr[3] = 0x00;
    net->mac_addr[4] = 0x00;
    net->mac_addr[5] = 0x01;
    net->opaque = slirp_state;
    net->write_packet = slirp_write_packet;
    net->select_fill = slirp_select_fill1;
    net->select_poll = slirp_select_poll1;
    
    return net;
}

#endif /* CONFIG_SLIRP */


int fdt_parse_virtionet(
    const void *fdt,
    reg_t* blkdev_addr,
    uint32_t* blkdev_int_id,
    const char *compatible) {
  int nodeoffset, rc, len;
  const fdt32_t *reg_p;

  nodeoffset = fdt_node_offset_by_compatible(fdt, -1, compatible);
  if (nodeoffset < 0)
    return nodeoffset;

  rc = fdt_get_node_addr_size(fdt, nodeoffset, blkdev_addr, NULL, "reg");
  if (rc < 0 || !blkdev_addr)
    return -ENODEV;

  reg_p = (fdt32_t *)fdt_getprop(fdt, nodeoffset, "interrupts", &len);
  if (blkdev_int_id) {
    if (reg_p)
      *blkdev_int_id = fdt32_to_cpu(*reg_p);
    else
      *blkdev_int_id = VIRTIO_NET_IRQ;
  }

  return 0;
}

virtionet_t::virtionet_t(
      const simif_t* sim,
      abstract_interrupt_controller_t *intctrl,
      uint32_t interrupt_id,
      std::vector<std::string> sargs)
  : virtio_base_t(sim, intctrl, interrupt_id, sargs)
{
  std::map<std::string, std::string> argmap;

  for (auto arg : sargs) {
    size_t eq_idx = arg.find('=');
    if (eq_idx != std::string::npos) {
      argmap.insert(std::pair<std::string, std::string>(arg.substr(0, eq_idx), arg.substr(eq_idx+1)));
    }
  }

  std::string driver;
  std::string hostfwd;
  
  auto it = argmap.find("driver");
  if (it == argmap.end()) {
    // invalid block device.
    printf("Virtio net  device plugin INIT ERROR: `path` argument not specified.\n"
            "Please use spike option --device=virtio9p,path=/path/to/folder to use an exist host filesystem folder path.\n");
    exit(1);
  }
  else {
    driver = it->second;
  }

  if(driver == "user"){
    auto it = argmap.find("hostfwd");
      if (it == argmap.end()) {
        // invalid block device.
        printf("Virtio net  device plugin INIT ERROR: `ifname` argument not specified.\n"
                "Please use spike option --device=virtio9p,path=/path/to/folder to use an exist host filesystem folder path.\n");
        exit(1);
      }
      else {
        hostfwd = it->second;
      }
  }

  int irq_num;
  VIRTIOBusDef vbus_s, *vbus = &vbus_s;

  EthernetDevice * net;
  if (driver == "user") {
    net = (EthernetDevice *) slirp_open();
    if (!net){
      printf("Virtio net disk fs device plugin INIT ERROR: `path` %s must be a directory\n", driver.c_str());
      exit(1);
    }
  } 
  
  Slirp * slirp_ptr = (Slirp *) net->opaque;

  memset(vbus, 0, sizeof(*vbus));
  vbus->addr = VIRTIO_NET_BASE;
  irq_num  = VIRTIO_NET_IRQ;
  irq = new IRQSpike(intctrl, irq_num);
  vbus->irq = irq;

  virtio_dev = virtio_net_init(vbus, net, sim);

  slirp_hostfwd(slirp_ptr, hostfwd.c_str(), NULL);

  vbus->addr += VIRTIO_SIZE;

}

virtionet_t::~virtionet_t() {
    if (irq) delete irq;
}


std::string virtionet_generate_dts(const sim_t* sim, const std::vector<std::string>& args) {
  std::stringstream s;
  s << std::hex 
    << "    virtionet: virtio@" << VIRTIO_NET_BASE << " {\n"
    << "      compatible = \"virtio,mmio\";\n"
       "      interrupt-parent = <&PLIC>;\n"
       "      interrupts = <" << std::dec << VIRTIO_NET_IRQ;
    reg_t virtio9pbs = VIRTIO_NET_BASE;
    reg_t virtio9psz = VIRTIO_SIZE;
  s << std::hex << ">;\n"
       "      reg = <0x" << (virtio9pbs >> 32) << " 0x" << (virtio9pbs & (uint32_t)-1) <<
                   " 0x" << (virtio9psz >> 32) << " 0x" << (virtio9psz & (uint32_t)-1) << ">;\n"
       "    };\n";
    return s.str();
}

virtionet_t* virtionet_parse_from_fdt(
  const void* fdt, const sim_t* sim, reg_t* base,
    std::vector<std::string> sargs)
{
  uint32_t blkdev_int_id;
  if (fdt_parse_virtionet(fdt, base, &blkdev_int_id, "virtio,mmio") == 0) {
    abstract_interrupt_controller_t* intctrl = sim->get_intctrl();
    return new virtionet_t(sim, intctrl, blkdev_int_id, sargs);
  } else {
    return nullptr;
  }
}

REGISTER_DEVICE(virtionet, virtionet_parse_from_fdt, virtionet_generate_dts);