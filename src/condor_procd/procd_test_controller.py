#!/usr/bin/python

import fcntl
import os
import signal
import socket
import subprocess
import sys

# helper function for setting FDs close-on-exec
def close_on_exec(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFD)
    fcntl.fcntl(fd, fcntl.F_SETFD, flags | fcntl.FD_CLOEXEC)

# helper class used by DroneTree; each drone has a persistent UNIX
# domain socket connection with the Controller (this program); a
# drone will exit once this connection is closed
class Drone:

    def __init__(self, drone_tree):
        s = drone_tree.sock.accept()[0]
        self.sock = s.makefile()
        s.close()
        close_on_exec(self.sock.fileno())
        self.pid = self.sock.readline().rstrip()

    def spawn(self, tag):
        self.sock.write('SPAWN ' + tag + '\n')
        self.sock.flush()

    def __del__(self):
        self.sock.close()

# class that allows for the creation and management of a tree of
# "drones"; drones are referred to by clients of this class using
# "tags" selected by the caller via the constructor and the spawn()
# method
class DroneTree:

    # creates a drone tree object and its initial process; the
    # child_callback gets called after the initial drone is forked
    # but before it execs; it's really just a hack to allow the
    # initial drone to be able to fork off the ProcD
    def __init__(self, drone_path, uds_path, init_tag, child_callback = None):

        self.drone_path = drone_path
        self.uds_path = uds_path

        # set up UNIX domain socket for drone communication
	self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0)
        if os.path.exists(uds_path):
            os.unlink(uds_path)
        self.sock.bind(uds_path)
        self.sock.listen(5)
        close_on_exec(self.sock.fileno())

        # fork off the initial drone
        self.init_pid = os.fork()
        if self.init_pid == 0:
            if child_callback is not None:
                child_callback()
            os.execl(drone_path,
                     os.path.basename(drone_path),
                     uds_path,
                     init_tag)

        # initialize the dict-of-all-drones
        self.drones = {init_tag: Drone(self)}

    # get the PID of a drone given its tag
    def lookup(self, tag):
        return self.drones[tag].pid

    def spawn(self, tag, parent_tag):
        if tag in self.drones:
            raise Exception('tag ' + tag + ' already exists')
        self.drones[parent_tag].spawn(tag)
        self.drones[tag] = Drone(self)

    def kill(self, tag):
        pid = int(self.drones[tag].pid)
        del self.drones[tag]
        if pid == self.init_pid:
            os.waitpid(pid, 0)

# simple class to represent ProcD state that provides methods
# for printing out the state and comparing two state objects;
# it's the reponsibilty of these methods' callers to get each
# passed-in object's internal state into the expected form
class ProcDState:

    def dump(self):
        def f(fam, indent):
            parent = None
            if fam.parent is not None:
                parent = fam.parent.id
            print '%s%s: %s (W: %s, P: %s)' % (indent,
                                               fam.id,
                                               fam.procs,
                                               fam.watcher,
                                               parent)
            for subfam in fam.children:
                f(subfam, indent + '    ')
        f(self, '')

    def equals(self, other):
        if self.id != other.id:
            return False
        if self.watcher != other.watcher:
            return False
        if self.parent is None:
            if other.parent is not None:
                return False
        else:
            if other.parent is None:
                return False
            if self.parent.id != other.parent.id:
                return False
        if len(self.children) != len(other.children):
            return False
        key = lambda x: x.id
        my_children = sorted(self.children, key = key)
        her_children = sorted(other.children, key = key)
        for mine, hers in zip(my_children, her_children):
            if not mine.equals(hers):
                return False
        return True

# class for maintaining a reference of what the ProcD's state
# should be; the "state" member can by used with the ProcDState
# methods above
class ProcDReference:

    def __init__(self, init_proc):

        self.state = ProcDState()
        self.state.id = init_proc
        self.state.procs = set([init_proc])
        self.state.parent = None
        self.state.children = set()
        self.state.watcher = init_proc

        # initialize some dictionaries for easy lookup of families
        self.families_by_id = {init_proc: self.state}
        self.families_by_proc = {init_proc: self.state}

    def spawn(self, proc, parent_proc):

        # add proc to same family as parent
        fam = self.families_by_proc[parent_proc]
        fam.procs.add(proc)

        self.families_by_proc[proc] = fam

    def kill(self, proc):

        # remove proc from its family
        self.families_by_proc[proc].procs.remove(proc)

        # keep lookup-by-proc dict up to date
        del self.families_by_proc[proc]

        # unregister any now-unwatched families
        def f(fam):
            if fam.watcher is not None:
                if fam.watcher not in self.families_by_proc:
                    self.unregister(fam.id)
            for subfam in fam.children:
                f(subfam)
        f(self.state)

    def register(self, proc, watcher):

        old_fam = self.families_by_proc[proc]

        new_fam = ProcDState()
        new_fam.id = proc
        new_fam.procs = set([proc])
        new_fam.parent = old_fam
        new_fam.children = set()
        new_fam.watcher = watcher

        old_fam.procs.remove(proc)
        old_fam.children.add(new_fam)

        self.families_by_id[proc] = new_fam
        self.families_by_proc[proc] = new_fam

    def unregister(self, id):

        fam = self.families_by_id[id]

        # parent family inherits unregistered family's procs, children
        fam.parent.procs |= fam.procs
        fam.parent.children |= fam.children

        fam.parent.children.remove(fam)

        # keep lookup tables consistent
        del self.families_by_id[id]
        for proc in fam.procs:
            self.families_by_proc[proc] = fam.parent


# class for interacting with the ProcD we're testing
class ProcDInterface:

    def __init__(self, procd_ctl_path, procd_addr):
        self.procd_ctl_path = procd_ctl_path
        self.procd_addr = procd_addr

    def register(self, proc, watcher):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'REGISTER_FAMILY',
                               str(proc),
                               str(watcher),
                               '-1'))
        if ret != 0:
            raise Exception('error result from procd_ctl REGISTER: ' + ret)

    def unregister(self, proc):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'UNREGISTER_FAMILY',
                               str(proc)))
        if ret != 0:
            raise Exception('error result from procd_ctl UNREGISTER: ' + ret)

    def snapshot(self):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'SNAPSHOT'))
        if ret != 0:
            raise Exception('error result from procd_ctl SNAPSHOT: ' + ret)

    def quit(self):
        ret = subprocess.call((self.procd_ctl_path,
                               self.procd_addr,
                               'QUIT'))
        if ret != 0:
            raise Exception('error result from procd_ctl QUIT: ' + ret)

    # sucks the state out of the ProcD and returns a ProcDState object
    def dump(self):
        p = subprocess.Popen((self.procd_ctl_path, self.procd_addr, 'DUMP'),
                             stdout = subprocess.PIPE)
        d = {}
        while True:
            line = p.stdout.readline()
            if not line:
                break
            tokens = line.split()
            pid = tokens[0]
            family = tokens[5]
            watcher = tokens[6]
            if watcher == '0':
                watcher = None
            parent_family = tokens[7]
            if family not in d:
                d[family] = ProcDState()
                d[family].id = family
                d[family].procs = set([pid])
                d[family].watcher = watcher
                d[family].children = set()
                if parent_family == '0':
                    ret = d[family]
                    d[family].parent = None
                else:
                    d[family].parent = d[parent_family]
                    d[parent_family].children.add(d[family])
            else:
                d[family].procs.add(pid)
        if p.wait() != 0:
            raise Exception('error result from procd_ctl DUMP: ' +
                            p.returncode)
        return ret

# top-level class to bring all the above together
class ProcDTester:

    def __init__(self):

        self.procd_path = './condor_procd'
        self.procd_addr = 'procd.pipe'
        self.procd_ctl_path = './procd_ctl'
        self.drone_path = './procd_test_drone.py'
        self.drone_addr = 'drone.sock'

        self.drone_tree = DroneTree(self.drone_path,
                                    self.drone_addr,
                                    'INIT',
                                    self.start_procd)

        self.procd_interface = ProcDInterface(self.procd_ctl_path,
                                              self.procd_addr)

        drone_pid = self.drone_tree.lookup('INIT')
        self.procd_reference = ProcDReference(drone_pid)

        # our start_procd method (see comments below) has by now
        # dropped the PID of the ProcD into a file; we now read it
        # in and update our reference state with it
        f = open('procd_pid', 'r')
        procd_pid = f.read().rstrip()
        f.close()
        os.unlink('procd_pid')
        self.procd_reference.spawn(procd_pid, drone_pid)

        # we keep a tag-to-family mapping; we can't just rely on
        # DroneTree's tag-to-PID mapping since a family still
        # exists after it's initial process has exited
        self.ids_by_tag = {}

    # this is given as the "child_callback" parameter to the
    # DroneTree constructor; the result is that the ProcD is
    # forked as a child of the initial drone (which is what we
    # want since the ProcD currently considers its parent to
    # be the root of its process tree)
    def start_procd(self):
        pid = os.fork()
        if pid == 0:
            os.execl(self.procd_path,
                     os.path.basename(self.procd_path),
                     '-A',
                     self.procd_addr,
                     '-S',
                     '-1')
        else:
            # record the ProcD's PID, which our constructor will
            # pick up
            f = open('procd_pid', 'w')
            f.write('%s\n' % pid)
            f.close()

    def stop_procd(self):
        self.procd_interface.quit()

    def spawn(self, tag, parent_tag):
        self.drone_tree.spawn(tag, parent_tag)
        self.procd_reference.spawn(self.drone_tree.lookup(tag),
                                   self.drone_tree.lookup(parent_tag))

    def kill(self, tag):
        pid = self.drone_tree.lookup(tag)
        self.drone_tree.kill(tag)
        self.procd_reference.kill(pid)

    def register(self, tag, watcher):
        proc = self.drone_tree.lookup(tag)
        if watcher is not None:
            watcher = self.drone_tree.lookup(watcher)
        self.procd_interface.register(proc, watcher)
        self.procd_reference.register(proc, watcher)
        self.ids_by_tag[tag] = proc

    def unregister(self, tag):
        id = self.ids_by_tag[tag]
        del self.ids_by_tag[tag]
        self.procd_interface.unregister(id)
        self.procd_reference.unregister(id)

    def snapshot(self):
        self.procd_interface.snapshot()

    def show_state(self, procd_state = None):
        if procd_state is None:
            procd_state = self.procd_interface.dump()
        print '------ ProcD State ------'
        procd_state.dump()
        print '------ Reference State ------'
        self.procd_reference.state.dump()
        print '------ Tag-to-PID Mappings ------'
        for tag, drone in sorted(self.drone_tree.drones.iteritems()):
            print '%s: %s' % (tag, drone.pid)
        print '------ Tag-to-Family Mappings ------'
        for tag, id in sorted(self.ids_by_tag.iteritems()):
            print '%s: %s' % (tag, id)
        print '------------------------------------'

    def check_state(self):
        state = self.procd_interface.dump()
        if not state.equals(self.procd_reference.state):
            print 'ProcD state is incorrect!'
            self.show_state(state)
            return False
        return True

    # run the test; commands are read from the given file;
    # if interactive is True, the test will wait for "Enter"
    # to be pressed in between each command
    def run(self, filename, interactive):
        f = open(filename, 'r')
        while True:
            line = f.readline()
            tokens = line.split()
            if not tokens:
                break
            if interactive:
                self.show_state()
                print 'Next command: ' + line.rstrip()
                raw_input()
            if tokens[0] == 'SPAWN':
                self.spawn(tokens[1], tokens[2])
                ok = True
            elif tokens[0] == 'KILL':
                self.kill(tokens[1])
                ok = True
            elif tokens[0] == 'REGISTER':
                watcher = None
                if len(tokens) > 2:
                    watcher = tokens[2]
                self.register(tokens[1], watcher)
                ok = self.check_state()
            elif tokens[0] == 'UNREGISTER':
                self.unregister(tokens[1])
                ok = self.check_state()
            elif tokens[0] == 'SNAPSHOT':
                self.snapshot()
                ok = self.check_state()
            if not ok:
                break
        f.close()

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'usage: %s <file>' % sys.argv[0]
        sys.exit(1)
    tester = ProcDTester()
    tester.run(sys.argv[1], True)
    tester.stop_procd()
