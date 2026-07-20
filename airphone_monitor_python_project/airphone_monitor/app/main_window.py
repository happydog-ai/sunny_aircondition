from __future__ import annotations

import json
from collections import deque
from datetime import datetime
from pathlib import Path

import pyqtgraph as pg
from PySide6.QtGui import QAction, QCloseEvent
from PySide6.QtWidgets import (
    QAbstractItemView, QCheckBox, QComboBox, QDoubleSpinBox, QFileDialog,
    QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit,
    QMainWindow, QMessageBox, QPlainTextEdit, QPushButton, QSpinBox,
    QStatusBar, QTabWidget, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget,
)
from serial.tools import list_ports

from app.data_logger import CsvDataLogger
from app.register_map import RegisterMap
from app.widgets import ValueCard
from app.worker import SerialWorker

BASE_DIR = Path(__file__).resolve().parent.parent
REGISTER_MAP_PATH = BASE_DIR / "config" / "register_map.json"


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("AirPhone 暖通控制板监控程序")
        self.resize(1250, 820)
        self.register_map = RegisterMap(REGISTER_MAP_PATH)
        self.worker = SerialWorker(REGISTER_MAP_PATH)
        self.logger = CsvDataLogger()
        self.connected = False
        self.led_state = False
        self.led_running = False
        self.last_state: dict = {}
        self.x = deque(maxlen=300); self.ambient_temp = deque(maxlen=300); self.target_temp = deque(maxlen=300); self.sample = 0
        self._build_ui(); self._connect_signals(); self.refresh_ports(); self._set_connected(False)

    def _build_ui(self) -> None:
        central = QWidget(); root = QVBoxLayout(central)
        root.addWidget(self._connection_bar())
        self.tabs = QTabWidget()
        self.tabs.addTab(self._dashboard(), "实时监控")
        self.tabs.addTab(self._controls(), "执行器控制")
        self.tabs.addTab(self._parameters(), "参数 / EEPROM")
        self.tabs.addTab(self._manual(), "手动读写")
        self.tabs.addTab(self._logs(), "通信日志")
        root.addWidget(self.tabs); self.setCentralWidget(central); self.setStatusBar(QStatusBar())
        toolbar = self.addToolBar("工具")
        self.record_action = QAction("开始记录CSV", self); self.record_action.setCheckable(True)
        self.record_action.triggered.connect(self.toggle_recording); toolbar.addAction(self.record_action)

    def _connection_bar(self) -> QWidget:
        box = QGroupBox("通信连接"); layout = QHBoxLayout(box)
        self.protocol_combo = QComboBox(); self.protocol_combo.addItem("当前 AA55 协议", "AA55"); self.protocol_combo.addItem("Modbus RTU", "MODBUS")
        self.port_combo = QComboBox(); self.refresh_button = QPushButton("扫描串口")
        self.baud_combo = QComboBox(); self.baud_combo.addItems(["9600", "19200", "38400", "57600", "115200"])
        self.parity_combo = QComboBox(); self.parity_combo.addItem("无校验", "N"); self.parity_combo.addItem("偶校验", "E"); self.parity_combo.addItem("奇校验", "O")
        self.address_spin = QSpinBox(); self.address_spin.setRange(1, 247); self.address_spin.setValue(1)
        self.poll_spin = QSpinBox(); self.poll_spin.setRange(100, 5000); self.poll_spin.setValue(self.register_map.poll_interval_ms); self.poll_spin.setSuffix(" ms")
        self.poll_checkbox = QCheckBox("自动轮询"); self.poll_checkbox.setChecked(True)
        self.connect_button = QPushButton("连接"); self.connection_label = QLabel("● 未连接")
        for title, widget in (("协议", self.protocol_combo), ("串口", self.port_combo), ("波特率", self.baud_combo), ("校验", self.parity_combo), ("地址", self.address_spin), ("轮询", self.poll_spin)):
            layout.addWidget(QLabel(title)); layout.addWidget(widget)
        layout.addWidget(self.refresh_button); layout.addWidget(self.poll_checkbox); layout.addWidget(self.connect_button); layout.addWidget(self.connection_label); layout.addStretch()
        return box

    def _dashboard(self) -> QWidget:
        page = QWidget(); layout = QVBoxLayout(page); grid = QGridLayout()
        definitions = [("bus_voltage", "母线电压", "V"), ("ambient_temp", "环境温度", "℃"), ("target_temp", "设定温度", "℃"), ("fan_rpm", "风机转速", "rpm"), ("fault_code", "故障码", ""), ("led", "调试LED", ""), ("system_enable", "系统使能", "")]
        self.cards = {}
        for i, (name, title, unit) in enumerate(definitions):
            card = ValueCard(title, unit); self.cards[name] = card; grid.addWidget(card, i // 5, i % 5)
        layout.addLayout(grid)
        self.plot = pg.PlotWidget(title="温度实时曲线"); self.plot.showGrid(x=True, y=True); self.plot.setLabel("left", "温度", units="℃"); self.plot.setLabel("bottom", "采样点")
        self.curve_ambient = self.plot.plot(name="环境温度", pen="g"); self.curve_target = self.plot.plot(name="设定温度", pen="r"); self.plot.addLegend(); layout.addWidget(self.plot, 1)
        return page

    def _controls(self) -> QWidget:
        page = QWidget(); layout = QVBoxLayout(page)
        coil_box = QGroupBox("Modbus开关量"); grid = QGridLayout(coil_box); self.coil_checks = {}
        definitions = [("系统使能", "system_enable", 0), ("风机使能", "fan_enable", 2)]
        for row, (title, name, address) in enumerate(definitions):
            check = QCheckBox(title); button = QPushButton("写入"); button.clicked.connect(lambda _=False, a=address, c=check: self.worker.submit("write_coil", address=a, enabled=c.isChecked()))
            self.coil_checks[name] = check; grid.addWidget(check, row, 0); grid.addWidget(QLabel(f"0x{address:04X}"), row, 1); grid.addWidget(button, row, 2)
        aa_box = QGroupBox("AA55协议调试"); aa_layout = QVBoxLayout(aa_box)
        btn_row = QHBoxLayout()
        self.ping_button = QPushButton("测试连接"); self.ping_button.clicked.connect(self._on_ping)
        self.version_button = QPushButton("获取版本号"); self.version_button.clicked.connect(self._on_get_version)
        self.led_button = QPushButton("打开LED"); self.led_button.clicked.connect(self._on_toggle_led)
        btn_row.addWidget(self.ping_button); btn_row.addWidget(self.version_button); btn_row.addWidget(self.led_button); btn_row.addStretch()
        aa_layout.addLayout(btn_row)
        status_row = QHBoxLayout()
        self.connection_status_label = QLabel("连接状态：未测试")
        self.version_label = QLabel("设备版本：--")
        self.led_status_label = QLabel("LED状态：未知")
        status_row.addWidget(self.connection_status_label); status_row.addWidget(self.version_label); status_row.addWidget(self.led_status_label); status_row.addStretch()
        aa_layout.addLayout(status_row)
        reg_box = QGroupBox("执行器参数（Modbus保持寄存器）"); form = QFormLayout(reg_box)
        for title, address, maximum in (("风机PWM（‰）", 3, 1000),):
            row = QWidget(); h = QHBoxLayout(row); h.setContentsMargins(0, 0, 0, 0); spin = QSpinBox(); spin.setRange(0, maximum); button = QPushButton("写入"); button.clicked.connect(lambda _=False, a=address, s=spin: self.worker.submit("write_register", address=a, value=s.value())); h.addWidget(spin); h.addWidget(button); form.addRow(title, row)
        layout.addWidget(coil_box); layout.addWidget(aa_box); layout.addWidget(reg_box); layout.addStretch(); return page

    def _parameters(self) -> QWidget:
        page = QWidget(); layout = QVBoxLayout(page)
        self.param_table = QTableWidget(0, 7); self.param_table.setHorizontalHeaderLabels(["名称", "地址", "当前值", "单位", "新值", "范围", "写入"]); self.param_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        items = self.register_map.items("holding_registers"); self.param_table.setRowCount(len(items)); self.param_rows = {}
        for row, item in enumerate(items):
            self.param_rows[item.name] = row; self.param_table.setItem(row, 0, QTableWidgetItem(item.label)); self.param_table.setItem(row, 1, QTableWidgetItem(f"0x{item.address:04X}")); self.param_table.setItem(row, 2, QTableWidgetItem("--")); self.param_table.setItem(row, 3, QTableWidgetItem(item.unit))
            edit = QDoubleSpinBox(); edit.setDecimals(3); edit.setRange(item.minimum if item.minimum is not None else -65535, item.maximum if item.maximum is not None else 65535); self.param_table.setCellWidget(row, 4, edit)
            self.param_table.setItem(row, 5, QTableWidgetItem(f"{item.minimum}～{item.maximum}")); button = QPushButton("写入"); button.setEnabled(item.writable); button.clicked.connect(lambda _=False, it=item, ed=edit: self._write_mapped(it, ed)); self.param_table.setCellWidget(row, 6, button)
        save = QPushButton("保存参数到EEPROM"); save.clicked.connect(lambda: self.worker.submit("write_coil", address=6, enabled=True)); layout.addWidget(self.param_table); layout.addWidget(save); return page

    def _manual(self) -> QWidget:
        page = QWidget(); form = QFormLayout(page)
        self.manual_op = QComboBox(); self.manual_op.addItems(["PING", "GET_VERSION", "ECHO", "GET_STATUS", "01 读线圈", "02 读离散输入", "03 读保持寄存器", "04 读输入寄存器", "05 写单线圈", "06 写单寄存器", "10 写多个寄存器"])
        self.manual_addr = QSpinBox(); self.manual_addr.setRange(0, 65535); self.manual_count = QSpinBox(); self.manual_count.setRange(1, 125); self.manual_data = QLineEdit(); self.manual_data.setPlaceholderText("ECHO填HEX；写寄存器填数值或逗号分隔列表")
        button = QPushButton("执行"); button.clicked.connect(lambda: self.worker.submit("manual", operation=self.manual_op.currentText(), address=self.manual_addr.value(), count=self.manual_count.value(), data=self.manual_data.text()))
        self.manual_result = QPlainTextEdit(); self.manual_result.setReadOnly(True)
        form.addRow("功能", self.manual_op); form.addRow("地址", self.manual_addr); form.addRow("数量", self.manual_count); form.addRow("数据", self.manual_data); form.addRow(button); form.addRow("结果", self.manual_result); return page

    def _logs(self) -> QWidget:
        page = QWidget(); layout = QVBoxLayout(page); top = QHBoxLayout(); clear = QPushButton("清空"); clear.clicked.connect(lambda: self.log_text.clear()); save = QPushButton("导出日志"); save.clicked.connect(self.save_log); self.auto_scroll = QCheckBox("自动滚动"); self.auto_scroll.setChecked(True); self.stats = QLabel("TX 0 | RX 0 | 错误 0 | 轮询 0")
        top.addWidget(clear); top.addWidget(save); top.addWidget(self.auto_scroll); top.addStretch(); top.addWidget(self.stats)
        self.log_text = QPlainTextEdit(); self.log_text.setReadOnly(True); self.log_text.setMaximumBlockCount(10000); self.log_text.setStyleSheet("font-family:Consolas,monospace;")
        layout.addLayout(top); layout.addWidget(self.log_text); return page

    def _connect_signals(self) -> None:
        self.refresh_button.clicked.connect(self.refresh_ports); self.connect_button.clicked.connect(self.toggle_connection)
        self.poll_checkbox.toggled.connect(lambda v: self.worker.submit("set_polling", enabled=v)); self.poll_spin.valueChanged.connect(lambda v: self.worker.submit("set_poll_interval", milliseconds=v))
        self.worker.connected.connect(self.on_connected); self.worker.disconnected.connect(self.on_disconnected); self.worker.status_updated.connect(self.on_status); self.worker.command_finished.connect(self.on_command); self.worker.communication_error.connect(self.on_error); self.worker.raw_log.connect(self.on_raw); self.worker.statistics_updated.connect(self.on_stats)

    def refresh_ports(self) -> None:
        current = self.port_combo.currentData(); self.port_combo.clear()
        for port in sorted(list_ports.comports(), key=lambda x: x.device): self.port_combo.addItem(f"{port.device} — {port.description}", port.device)
        if current:
            index = self.port_combo.findData(current)
            if index >= 0: self.port_combo.setCurrentIndex(index)

    def toggle_connection(self) -> None:
        if self.connected: self.worker.submit("disconnect"); return
        port = self.port_combo.currentData()
        if not port: QMessageBox.warning(self, "提示", "没有选择可用串口"); return
        self.connect_button.setEnabled(False); self.worker.submit("connect", port=port, baudrate=int(self.baud_combo.currentText()), parity=self.parity_combo.currentData(), address=self.address_spin.value(), protocol=self.protocol_combo.currentData(), timeout=0.3)

    def on_connected(self, info: dict) -> None:
        self.connected = True; self._set_connected(True)
        ver = info.get("version")
        extra = f"，版本{ver}" if ver else ""; self.statusBar().showMessage(f"已连接{info['port']}，协议{info['protocol']}{extra}")
        self.connection_status_label.setText("连接状态：已连接")

    def on_disconnected(self) -> None:
        self.connected = False; self._set_connected(False); self.statusBar().showMessage("已断开")

    def _set_connected(self, connected: bool) -> None:
        self.connect_button.setEnabled(True); self.connect_button.setText("断开" if connected else "连接"); self.connection_label.setText("● 已连接" if connected else "● 未连接"); self.connection_label.setStyleSheet("color:#1b5e20;font-weight:600;" if connected else "color:#b71c1c;font-weight:600;")
        for widget in (self.protocol_combo, self.port_combo, self.baud_combo, self.parity_combo, self.address_spin, self.refresh_button): widget.setEnabled(not connected)

    def on_status(self, state: dict) -> None:
        self.last_state.update(state)
        for name, card in self.cards.items():
            if name in state: card.set_value(state[name])
        for name, check in self.coil_checks.items():
            if name in state: check.setChecked(bool(state[name]))
        if "led" in state:
            led_state = bool(state["led"])
            self.led_state = led_state
            self.led_status_label.setText(f"LED状态：{'已打开' if led_state else '已关闭'}")
            if not self.led_running:
                self.led_button.setText("关闭LED" if led_state else "打开LED")
        for name, row in self.param_rows.items():
            if name in state: self.param_table.item(row, 2).setText(str(state[name]))
        self.sample += 1
        if "ambient_temp" in state or "target_temp" in state:
            self.x.append(self.sample); self.ambient_temp.append(float(state.get("ambient_temp", float("nan")))); self.target_temp.append(float(state.get("target_temp", float("nan")))); self.curve_ambient.setData(list(self.x), list(self.ambient_temp)); self.curve_target.setData(list(self.x), list(self.target_temp))
        if self.logger.active: self.logger.write(self.last_state)

    def on_command(self, name: str, result) -> None:
        if name == "_connect_failed":
            self.connect_button.setEnabled(True)
            return
        if isinstance(result, dict):
            self._handle_aa55_result(name, result)
        else:
            self.statusBar().showMessage(f"{name}执行成功：{result}", 5000)
        if name == "manual": self.manual_result.appendPlainText(json.dumps(result, ensure_ascii=False, indent=2, default=str))

    def on_error(self, message: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]; self.log_text.appendPlainText(f"{stamp} ERROR {message}"); self.statusBar().showMessage(message, 8000)

    def on_raw(self, direction: str, text: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]; self.log_text.appendPlainText(f"{stamp} {direction:<2} {text}")
        if self.auto_scroll.isChecked(): self.log_text.verticalScrollBar().setValue(self.log_text.verticalScrollBar().maximum())

    def on_stats(self, s: dict) -> None:
        self.stats.setText(f"TX {s['tx_frames']} | RX {s['rx_frames']} | 错误 {s['errors']} | 轮询 {s['polls']}")

    def _on_ping(self) -> None:
        self.ping_button.setEnabled(False)
        self.worker.submit("ping")

    def _on_get_version(self) -> None:
        self.version_button.setEnabled(False)
        self.worker.submit("get_version")

    def _on_toggle_led(self) -> None:
        self.led_button.setEnabled(False)
        self.led_running = True
        self.worker.submit("set_led", enabled=not self.led_state)

    def _handle_aa55_result(self, name: str, result: dict) -> None:
        success = result["success"]
        elapsed = result.get("elapsed_ms", 0)
        if name == "ping":
            self.ping_button.setEnabled(True)
            if success:
                self.connection_status_label.setText("连接状态：连接正常")
                self.statusBar().showMessage(f"连接测试成功，耗时 {elapsed:.0f} ms", 5000)
            else:
                self.connection_status_label.setText("连接状态：失败")
                self.statusBar().showMessage(f"连接测试失败：{result['error']}", 5000)
        elif name == "get_version":
            self.version_button.setEnabled(True)
            if success:
                ver = result["data"]
                self.version_label.setText(f"设备版本：{ver}")
                self.statusBar().showMessage(f"获取版本成功：{ver}，耗时 {elapsed:.0f} ms", 5000)
            else:
                self.statusBar().showMessage(f"获取版本失败：{result['error']}", 5000)
        elif name == "set_led":
            self.led_button.setEnabled(True)
            self.led_running = False
            if success:
                self.led_state = result["data"]
                self.led_button.setText("关闭LED" if self.led_state else "打开LED")
                self.led_status_label.setText(f"LED状态：{'已打开' if self.led_state else '已关闭'}")
                self.statusBar().showMessage(f"LED{'打开' if self.led_state else '关闭'}成功，耗时 {elapsed:.0f} ms", 5000)
            else:
                self.statusBar().showMessage(f"LED操作失败：{result['error']}", 5000)

    def _write_mapped(self, item, edit: QDoubleSpinBox) -> None:
        try: raw = item.encode(edit.value())
        except ValueError as exc: QMessageBox.warning(self, "参数错误", str(exc)); return
        self.worker.submit("write_register", address=item.address, value=raw)

    def save_log(self) -> None:
        path, _ = QFileDialog.getSaveFileName(self, "导出通信日志", "communication.log", "日志文件 (*.log *.txt)")
        if path: Path(path).write_text(self.log_text.toPlainText(), encoding="utf-8")

    def toggle_recording(self, checked: bool) -> None:
        if checked:
            default = datetime.now().strftime("airphone_%Y%m%d_%H%M%S.csv"); path, _ = QFileDialog.getSaveFileName(self, "保存监控数据", default, "CSV文件 (*.csv)")
            if not path: self.record_action.setChecked(False); return
            self.logger.start(path); self.record_action.setText("停止记录CSV")
        else:
            self.logger.stop(); self.record_action.setText("开始记录CSV")

    def closeEvent(self, event: QCloseEvent) -> None:
        self.logger.stop(); self.worker.stop(); event.accept()
