/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef CONDOR_VM_UNIVERSE_TYPES_H_INCLUDE
#define CONDOR_VM_UNIVERSE_TYPES_H_INCLUDE

#define CONDOR_VM_UNIVERSE_XEN "xen"
#define CONDOR_VM_UNIVERSE_VMWARE "vmware"
#define CONDOR_VM_UNIVERSE_KVM "kvm"


#define VM_CKPT_FILE_EXTENSION	".cckpt"
#define VM_AVAIL_UNLIMITED_NUM	10000

#define VM_UNIV_ARGUMENT_FILE	"condor.arg"

/* Running modes for VM GAHP Server */

enum
{
    VMGAHP_TEST_MODE=0,
    VMGAHP_STANDALONE_MODE,
    VMGAHP_KILL_MODE,
    VMGAHP_MODE_MAX // should always be last, if needed insert before.
};

/* Parameters in a result of VM GAHP STATUS command  */
#define VMGAHP_STATUS_COMMAND_STATUS	"STATUS"
#define VMGAHP_STATUS_COMMAND_PID		"PID"
#define VMGAHP_STATUS_COMMAND_MAC		"MAC"
#define VMGAHP_STATUS_COMMAND_IP		"IP"
#define VMGAHP_STATUS_COMMAND_CPUTIME	"CPUTIME"

/* Parameters for Xen kernel */
#define XEN_KERNEL_INCLUDED		"included"
#define XEN_KERNEL_HW_VT		"vmx"

/* variables for vm-gahp2 */


/* ClassAd Attributes for Xen */
#define VMPARAM_XEN_KERNEL			"vm_xen_kernel"
#define VMPARAM_XEN_INITRD			"vm_xen_initrd"
#define VMPARAM_XEN_ROOT			"vm_xen_root"
#define VMPARAM_XEN_DISK			"vm_xen_disk"
#define VMPARAM_XEN_KERNEL_PARAMS	"vm_xen_kernel_params"
#define VMPARAM_XEN_CDROM_DEVICE	"vm_xen_cdrom"
#define VMPARAM_XEN_TRANSFER_FILES	"vm_xen_transfer_files"
#define VMPARAM_XEN_BOOTLOADER		"vm_xen_bootloader"

/* ClassAd Attributes for KVM */
#define VMPARAM_KVM_DISK			"vm_kvm_disk"
#define VMPARAM_KVM_CDROM_DEVICE	"vm_kvm_cdrom"
#define VMPARAM_KVM_TRANSFER_FILES	"vm_kvm_transfer_files"

/* ClassAd Attributes for VMware */
#define VMPARAM_VMWARE_TRANSFER		"vm_vmware_transfer"
#define VMPARAM_VMWARE_SNAPSHOTDISK "vm_vmware_snapshot_disk"
#define VMPARAM_VMWARE_DIR			"vm_vmware_dir"
#define VMPARAM_VMWARE_VMX_FILE		"vm_vmware_vmx_file"
#define VMPARAM_VMWARE_VMDK_FILES	"vm_vmware_vmdk_files"

/* Extra ClassAd Attributes for VM */
#define VMPARAM_NO_OUTPUT_VM			"vm_no_output_wm"
#define VMPARAM_CDROM_FILES				"vm_cdrom"
#define VMPARAM_TRANSFER_CDROM_FILES	"vm_transfer_cdrom_files"
#define VMPARAM_BRIDGE_INTERFACE	    "vm_bridge_interface"

#endif

