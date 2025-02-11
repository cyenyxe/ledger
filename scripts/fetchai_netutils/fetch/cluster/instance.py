import os
import subprocess
import fnmatch


class Instance(object):
    def __init__(self, cmd, root):
        self._root = root
        self._exit_code = None
        self._cmd = cmd
        self._log_path = os.path.join(root, 'node.log')
        self._logfile = None
        self._process = None

    def start(self):
        self._logfile = open(self._log_path, 'w')
        self._process = subprocess.Popen(
            self._cmd, cwd=self._root, stdout=self._logfile, stderr=subprocess.STDOUT)

    def stop(self, timeout=None):
        if self._process is not None:

            # terminate the process
            self._process.terminate()
            self._process.wait(timeout=timeout)

            # tear down the log file
            self._logfile.close()
            self._logfile = None

    def poll(self):
        if self._exit_code is not None:
            self._exit_code = self._process.poll()
        return self._exit_code

    @property
    def cmd(self):
        return ' '.join(self._cmd)

    @property
    def root(self):
        return self._root

    @property
    def log_path(self):
        return self._log_path


class ConstellationInstance(Instance):
    def __init__(self, app_path, port_start, root, clear_path=True):
        self._app_path = str(app_path)
        self._port_start = int(port_start)
        self._peers = []
        self._mining = False
        self._block_interval = 1000
        self._feature_flags = []
        self._standalone = False
        self._lanes = None
        self._slices = None

        # fake construct the instances
        super().__init__([], root)

        # updates the command line configuration
        self._update_cmd()

        # if requested clear the current directory
        if clear_path:
            for ext in ('*.db', '*.log'):
                for item in fnmatch.filter(os.listdir(root), ext):
                    os.remove(os.path.join(root, item))

    def add_peer(self, peer):
        print(self.p2p_address, '<=>', peer.p2p_address)

        self._peers.append(peer.p2p_address)
        self._update_cmd()

    def start(self):
        super().start()

        print('Constellation instance {} on pid {}'.format(
            self._port_start, self._process.pid))
        print(self._cmd)

    @property
    def p2p_address(self):
        return 'tcp://127.0.0.1:' + str(self._port_start + 1)

    @property
    def mining(self):
        return self._mining

    @mining.setter
    def mining(self, mining):
        self._mining = bool(mining)
        self._update_cmd()

    @property
    def standalone(self):
        return self._standalone

    @standalone.setter
    def standalone(self, standalone):
        self._standalone = bool(standalone)
        self._update_cmd()

    @property
    def block_interval(self):
        return self._block_interval

    @block_interval.setter
    def block_interval(self, block_interval):
        self._block_interval = block_interval
        self._update_cmd()

    @property
    def feature_flags(self):
        return self._feature_flags

    @feature_flags.setter
    def feature_flags(self, feature_flags):
        self._feature_flags = feature_flags
        self._update_cmd()

    @property
    def lanes(self):
        return self._lanes

    @lanes.setter
    def lanes(self, value):
        if value is None:
            self._lanes = None
        else:
            value = int(value)
            popcount = bin(value).count("1")
            if popcount != 1:
                raise RuntimeError('Lanes must be a valid power of 2')

            self._lanes = value
        self._update_cmd()

    @property
    def slices(self):
        return self._slices

    @slices.setter
    def slices(self, value):
        self._slices = None if value is None else int(value)
        self._update_cmd()

    def _update_cmd(self):
        cmd = [
            self._app_path,
            '-port', self._port_start,
        ]

        # add peers if required
        if len(self._peers) > 0:
            cmd += ['-peers', ','.join(self._peers)]

        # add the mining flag if needed
        if self._mining:
            if self._standalone:
                cmd += ['-standalone']

            cmd += ['-block-interval', self._block_interval]

        if self._lanes:
            cmd += ['-lanes', self._lanes]

        if self._slices:
            cmd += ['-slices', self._slices]

        if self._feature_flags:
            cmd += ['-experimental', ','.join(self._feature_flags)]

        self._cmd = list(map(str, cmd))

    # Append arbitrary flags
    def append_to_cmd(self, extra):
        self._cmd = [*self._cmd, *extra]
