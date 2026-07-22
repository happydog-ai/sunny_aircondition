from __future__ import annotations

import json
from collections import deque
from datetime import datetime
from pathlib import Path

import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QCloseEvent
from PySide6.QtWidgets import (
    QAbstractItemView, QCheckBox, QComboBox, QDoubleSpinBox, QFileDialog,
    QFormLayout, QFrame, QGridLayout, QHBoxLayout, QLabel, QLineEdit,
    QMainWindow, QMessageBox, QPlainTextEdit, QPushButton, QSizePolicy,
    QSpinBox, QStackedWidget, QStatusBar, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget,
)
from serial.tools import list_ports

from app.data_logger import CsvDataLogger
from app.register_map import RegisterMap
from app.widgets import ValueCard
from app.worker import SerialWorker

BASE_DIR = Path(__file__).resolve().parent.parent
REGISTER_MAP_PATH = BASE_DIR / "config" / "register_map.json"

PAGE_INFO = {
    0: ("实时监控", "实时查看母线电压、温度、风机转速等设备运行状态"),
    1: ("执行器控制", "控制开关量输出、风机 PWM 及压缩机调试操作"),
    2: ("参数 / EEPROM", "查看与修改保持寄存器参数，支持 EEPROM 读写测试"),
    3: ("手动读写", "通过 AA55 或 Modbus 协议手动读写任意寄存器地址"),
    4: ("通信日志", "查看串口通信原始数据的收发日志"),
}

STYLESHEET = """
QMainWindow {
    background: #F3F5F7;
}
QFrame#sidebar {
    background: #20252B;
    border: none;
}
QLabel#sidebarTitle {
    color: #FFFFFF;
    font-size: 15px;
    font-weight: 700;
    padding: 24px 20px 4px 20px;
}
QLabel#sidebarVersion {
    color: #6B7280;
    font-size: 11px;
    padding: 4px 20px 20px 20px;
}
QPushButton#navBtn {
    background: transparent;
    color: #9CA3AF;
    border: none;
    border-radius: 8px;
    padding: 10px 12px;
    text-align: left;
    font-size: 13px;
    font-weight: 500;
    margin: 0 8px;
}
QPushButton#navBtn:hover {
    background: #2C333B;
    color: #D1D5DB;
}
QPushButton#navBtn[active="true"] {
    background: #2C333B;
    color: #FFFFFF;
    font-weight: 600;
}

QFrame#header {
    background: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 12px;
}
QLabel#headerTitle {
    font-size: 18px;
    font-weight: 700;
    color: #1F2937;
}
QLabel#headerSubtitle {
    font-size: 12px;
    color: #6B7280;
    margin-top: 2px;
}

QFrame#card {
    background: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 12px;
}

QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
    background: #F9FAFB;
    border: 1px solid #D1D5DB;
    border-radius: 6px;
    padding: 5px 8px;
    min-height: 22px;
    font-size: 12px;
    color: #1F2937;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
}
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
    border-color: #2563EB;
}
QComboBox::drop-down {
    border: none;
    padding-right: 6px;
}

QCheckBox {
    font-size: 12px;
    color: #1F2937;
    spacing: 6px;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border-radius: 4px;
    border: 1px solid #D1D5DB;
    background: #F9FAFB;
}
QCheckBox::indicator:checked {
    background: #2563EB;
    border-color: #2563EB;
}

QPushButton {
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    border-radius: 6px;
    padding: 6px 16px;
    font-size: 12px;
}
QPushButton#primaryBtn {
    background: #20252B;
    color: #FFFFFF;
    border: none;
    font-weight: 600;
    min-height: 24px;
}
QPushButton#primaryBtn:hover {
    background: #374151;
}
QPushButton#primaryBtn:disabled {
    background: #D1D5DB;
    color: #9CA3AF;
}
QPushButton#dangerBtn {
    background: #D64545;
    color: #FFFFFF;
    border: none;
    font-weight: 600;
}
QPushButton#dangerBtn:hover {
    background: #B91C1C;
}
QPushButton#secondaryBtn {
    background: #F3F4F6;
    color: #374151;
    border: 1px solid #D1D5DB;
    font-weight: 500;
}
QPushButton#secondaryBtn:hover {
    background: #E5E7EB;
}
QPushButton#warningBtn {
    background: #FFF7ED;
    color: #D97706;
    border: 1px solid #FED7AA;
    font-weight: 500;
}
QPushButton#warningBtn:hover {
    background: #FFEDD5;
}
QPushButton#writeBtn {
    background: #EFF6FF;
    color: #2563EB;
    border: 1px solid #BFDBFE;
    font-weight: 500;
    padding: 5px 12px;
}
QPushButton#writeBtn:hover {
    background: #DBEAFE;
}

QLabel#connHint {
    font-size: 11px;
    color: #9CA3AF;
    padding: 0;
}
QLabel#connLabel {
    font-size: 12px;
    color: #6B7280;
    font-weight: 500;
}

QFrame#valueCard {
    background: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 12px;
}
QLabel#cardTitle {
    font-size: 12px;
    color: #6B7280;
}
QLabel#cardValue {
    font-size: 26px;
    font-weight: 700;
}

QTableWidget {
    background: #FFFFFF;
    border: 1px solid #E5E7EB;
    border-radius: 8px;
    gridline-color: #F3F4F6;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 12px;
}
QTableWidget::item {
    padding: 6px 8px;
    color: #1F2937;
}
QHeaderView::section {
    background: #F9FAFB;
    border: none;
    border-bottom: 2px solid #E5E7EB;
    padding: 8px;
    font-weight: 600;
    font-size: 12px;
    color: #6B7280;
}

QPlainTextEdit#logArea {
    background: #1F2937;
    color: #CBD5E1;
    border: none;
    border-radius: 8px;
    font-family: "Cascadia Code", "Consolas", "Courier New", monospace;
    font-size: 12px;
    padding: 12px;
    selection-background-color: #374151;
}

QScrollBar:vertical {
    background: transparent;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #CBD5E1;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover {
    background: #94A3B8;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background: transparent;
    height: 8px;
}
QScrollBar::handle:horizontal {
    background: #CBD5E1;
    border-radius: 4px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover {
    background: #94A3B8;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

QStatusBar {
    background: #FFFFFF;
    border-top: 1px solid #E5E7EB;
    font-size: 12px;
    color: #6B7280;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
}
QToolBar {
    background: #FFFFFF;
    border-bottom: 1px solid #E5E7EB;
    spacing: 8px;
    padding: 2px 8px;
}
"""


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("AirPhone 暖通控制板监控程序")
        self.resize(1280, 880)
        self.setMinimumSize(1024, 700)
        self.setStyleSheet(STYLESHEET)

        self.register_map = RegisterMap(REGISTER_MAP_PATH)
        self.worker = SerialWorker(REGISTER_MAP_PATH)
        self.logger = CsvDataLogger()
        self.connected = False
        self.led_state = False
        self.led_running = False
        self._compressor_on = False
        self.last_state: dict = {}
        self.x = deque(maxlen=300)
        self.ambient_temp = deque(maxlen=300)
        self.target_temp = deque(maxlen=300)
        self.sample = 0
        self._last_update: datetime | None = None

        self._build_ui()
        self._connect_signals()
        self.refresh_ports()
        self._set_connected(False)

    # ==================== UI Construction ====================

    def _build_ui(self) -> None:
        central = QWidget()
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        main_layout.addWidget(self._sidebar())

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(20, 20, 20, 20)
        right_layout.setSpacing(14)

        right_layout.addWidget(self._header())
        right_layout.addWidget(self._connection_card())

        self.pages = QStackedWidget()
        self.pages.addWidget(self._dashboard_page())
        self.pages.addWidget(self._controls_page())
        self.pages.addWidget(self._parameters_page())
        self.pages.addWidget(self._manual_page())
        self.pages.addWidget(self._logs_page())
        right_layout.addWidget(self.pages, 1)

        main_layout.addWidget(right, 1)
        self.setCentralWidget(central)
        self.setStatusBar(QStatusBar())

        toolbar = self.addToolBar("工具")
        self.record_action = QAction("开始记录CSV", self)
        self.record_action.setCheckable(True)
        self.record_action.triggered.connect(self.toggle_recording)
        toolbar.addAction(self.record_action)

        self._switch_page(0)

    def _sidebar(self) -> QWidget:
        sidebar = QFrame()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(220)

        layout = QVBoxLayout(sidebar)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        title = QLabel("AirPhone 暖通监控")
        title.setObjectName("sidebarTitle")
        layout.addWidget(title)

        version = QLabel("v1.0")
        version.setObjectName("sidebarVersion")
        layout.addWidget(version)

        sep = QFrame()
        sep.setFixedHeight(1)
        sep.setStyleSheet("background:#2C333B;border:none;margin:0 12px;")
        layout.addWidget(sep)
        layout.addSpacing(12)

        nav_items = [
            ("📡  实时监控", 0),
            ("⚙  执行器控制", 1),
            ("📋  参数 / EEPROM", 2),
            ("⌨  手动读写", 3),
            ("📄  通信日志", 4),
        ]

        self.nav_buttons: list[QPushButton] = []
        for label, idx in nav_items:
            btn = QPushButton(label)
            btn.setObjectName("navBtn")
            btn.setCursor(Qt.CursorShape.PointingHandCursor)
            btn.clicked.connect(lambda _checked, i=idx: self._switch_page(i))
            self.nav_buttons.append(btn)
            layout.addWidget(btn)

        layout.addStretch()
        return sidebar

    def _header(self) -> QWidget:
        bar = QFrame()
        bar.setObjectName("header")
        bar.setFixedHeight(68)

        layout = QHBoxLayout(bar)
        layout.setContentsMargins(20, 10, 20, 10)

        left = QVBoxLayout()
        left.setSpacing(2)
        self.header_title_label = QLabel("实时监控")
        self.header_title_label.setObjectName("headerTitle")
        self.header_subtitle_label = QLabel(PAGE_INFO[0][1])
        self.header_subtitle_label.setObjectName("headerSubtitle")
        left.addWidget(self.header_title_label)
        left.addWidget(self.header_subtitle_label)
        layout.addLayout(left)

        layout.addStretch()

        self.header_status_badge = QLabel("● 设备未连接")
        self.header_status_badge.setProperty("status", "disconnected")
        self._refresh_header_status()
        self.header_port_label = QLabel("")
        self.header_port_label.setStyleSheet("font-size:12px;color:#6B7280;margin-left:8px;")

        status_wrap = QHBoxLayout()
        status_wrap.setSpacing(0)
        status_wrap.addWidget(self.header_status_badge)
        status_wrap.addWidget(self.header_port_label)
        layout.addLayout(status_wrap)

        return bar

    def _connection_card(self) -> QWidget:
        card = QFrame()
        card.setObjectName("card")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(20, 16, 20, 14)
        layout.setSpacing(10)

        title_row = QHBoxLayout()
        card_title = QLabel("设备连接")
        card_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        title_row.addWidget(card_title)
        title_row.addStretch()
        layout.addLayout(title_row)

        row1 = QHBoxLayout()
        row1.setSpacing(16)

        self.protocol_combo = QComboBox()
        self.protocol_combo.addItem("当前 AA55 协议", "AA55")
        self.protocol_combo.addItem("Modbus RTU", "MODBUS")

        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(120)

        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["9600", "19200", "38400", "57600", "115200"])
        self.baud_combo.setCurrentText("9600")

        self.parity_combo = QComboBox()
        self.parity_combo.addItem("无校验", "N")
        self.parity_combo.addItem("偶校验", "E")
        self.parity_combo.addItem("奇校验", "O")

        self.address_spin = QSpinBox()
        self.address_spin.setRange(1, 247)
        self.address_spin.setValue(1)
        self.address_spin.setMinimumWidth(70)

        self.poll_spin = QSpinBox()
        self.poll_spin.setRange(100, 5000)
        self.poll_spin.setValue(self.register_map.poll_interval_ms)
        self.poll_spin.setSuffix(" ms")
        self.poll_spin.setMinimumWidth(90)

        for label_text, widget in (
            ("协议", self.protocol_combo),
            ("串口", self.port_combo),
            ("波特率", self.baud_combo),
            ("校验", self.parity_combo),
            ("地址", self.address_spin),
            ("轮询", self.poll_spin),
        ):
            group = QVBoxLayout()
            group.setSpacing(4)
            hint = QLabel(label_text)
            hint.setObjectName("connHint")
            group.addWidget(hint)
            group.addWidget(widget)
            row1.addLayout(group)

        layout.addLayout(row1)

        row2 = QHBoxLayout()
        row2.setSpacing(12)

        self.refresh_button = QPushButton("扫描串口")
        self.refresh_button.setObjectName("secondaryBtn")

        self.poll_checkbox = QCheckBox("自动轮询")
        self.poll_checkbox.setChecked(True)

        self.connect_button = QPushButton("连接")
        self.connect_button.setObjectName("primaryBtn")

        self.connection_label = QLabel("● 未连接")
        self.connection_label.setStyleSheet("font-size:13px;color:#D64545;font-weight:600;")

        row2.addWidget(self.refresh_button)
        row2.addWidget(self.poll_checkbox)
        row2.addStretch()
        row2.addWidget(self.connect_button)
        row2.addWidget(self.connection_label)

        layout.addLayout(row2)
        return card

    def _switch_page(self, index: int) -> None:
        self.pages.setCurrentIndex(index)
        info = PAGE_INFO.get(index, ("", ""))
        self.header_title_label.setText(info[0])
        self.header_subtitle_label.setText(info[1])
        for i, btn in enumerate(self.nav_buttons):
            btn.setProperty("active", i == index)
            btn.style().unpolish(btn)
            btn.style().polish(btn)

    # ==================== Page Builders ====================

    def _dashboard_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(14)

        cards_widget = QWidget()
        cards_grid = QGridLayout(cards_widget)
        cards_grid.setContentsMargins(0, 0, 0, 0)
        cards_grid.setSpacing(12)

        definitions = [
            ("bus_voltage", "母线电压", "V", "#2563EB"),
            ("ambient_temp", "环境温度", "℃", "#2563EB"),
            ("target_temp", "设定温度", "℃", "#2563EB"),
            ("fan_rpm", "风机转速", "rpm", "#1F2937"),
            ("fault_code", "故障码", "", "#1F2937"),
            ("device_status", "设备状态", "", "#1F2937"),
        ]

        self.cards = {}
        for i, (name, title, unit, color) in enumerate(definitions):
            card = ValueCard(title, unit, color)
            self.cards[name] = card
            cards_grid.addWidget(card, i // 3, i % 3)

        layout.addWidget(cards_widget)

        bottom = QWidget()
        bottom_layout = QHBoxLayout(bottom)
        bottom_layout.setContentsMargins(0, 0, 0, 0)
        bottom_layout.setSpacing(12)

        chart_card = QFrame()
        chart_card.setObjectName("card")
        chart_layout = QVBoxLayout(chart_card)
        chart_layout.setContentsMargins(16, 16, 16, 16)
        chart_layout.setSpacing(8)

        chart_header = QHBoxLayout()
        chart_title = QLabel("温度趋势")
        chart_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        chart_sub = QLabel("最近 300 个采样点")
        chart_sub.setStyleSheet("font-size:11px;color:#6B7280;")
        chart_header.addWidget(chart_title)
        chart_header.addStretch()
        chart_header.addWidget(chart_sub)
        chart_layout.addLayout(chart_header)

        self.plot = pg.PlotWidget()
        self.plot.setBackground("#F8FAFC")
        pi = self.plot.getPlotItem()
        pi.showGrid(x=True, y=True, alpha=0.25)
        pi.setLabel("left", "温度", units="℃")
        pi.setLabel("bottom", "采样点")
        pi.getAxis("left").setPen(pg.mkPen(color="#CBD5E1"))
        pi.getAxis("bottom").setPen(pg.mkPen(color="#CBD5E1"))
        pi.getAxis("left").setTextPen(pg.mkPen(color="#6B7280"))
        pi.getAxis("bottom").setTextPen(pg.mkPen(color="#6B7280"))

        self.curve_ambient = self.plot.plot(
            name="环境温度", pen=pg.mkPen(color="#2563EB", width=2)
        )
        self.curve_target = self.plot.plot(
            name="设定温度", pen=pg.mkPen(color="#D97706", width=2, style=Qt.PenStyle.DashLine)
        )
        legend = pi.addLegend()
        legend.setBrush(pg.mkBrush(255, 255, 255, 220))

        chart_layout.addWidget(self.plot, 1)
        bottom_layout.addWidget(chart_card, 2)

        status_card = QFrame()
        status_card.setObjectName("card")
        status_layout = QVBoxLayout(status_card)
        status_layout.setContentsMargins(16, 16, 16, 16)
        status_layout.setSpacing(10)

        st_title = QLabel("设备状态概览")
        st_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        status_layout.addWidget(st_title)

        st_form = QFormLayout()
        st_form.setSpacing(8)

        self.dash_switch_status = QLabel("--")
        self.dash_switch_status.setStyleSheet("font-size:12px;color:#1F2937;font-weight:500;")
        self.dash_fan_status = QLabel("--")
        self.dash_fan_status.setStyleSheet("font-size:12px;color:#1F2937;font-weight:500;")
        self.dash_fault_status = QLabel("--")
        self.dash_fault_status.setStyleSheet("font-size:12px;color:#1F2937;font-weight:500;")
        self.dash_last_time = QLabel("--")
        self.dash_last_time.setStyleSheet("font-size:12px;color:#1F2937;font-weight:500;")
        self.dash_poll_status = QLabel("--")
        self.dash_poll_status.setStyleSheet("font-size:12px;color:#1F2937;font-weight:500;")

        for label_text, value_widget in (
            ("系统开关", self.dash_switch_status),
            ("风机状态", self.dash_fan_status),
            ("故障状态", self.dash_fault_status),
            ("最近通信", self.dash_last_time),
            ("轮询状态", self.dash_poll_status),
        ):
            lbl = QLabel(label_text)
            lbl.setStyleSheet("font-size:12px;color:#6B7280;")
            st_form.addRow(lbl, value_widget)

        status_layout.addLayout(st_form)
        status_layout.addStretch()
        bottom_layout.addWidget(status_card, 1)

        layout.addWidget(bottom, 1)
        return page

    def _controls_page(self) -> QWidget:
        page = QWidget()
        wrap = QVBoxLayout(page)
        wrap.setContentsMargins(0, 0, 0, 0)
        wrap.setSpacing(12)

        coil_card = QFrame()
        coil_card.setObjectName("card")
        coil_layout = QVBoxLayout(coil_card)
        coil_layout.setContentsMargins(20, 16, 20, 16)
        coil_layout.setSpacing(10)
        coil_title = QLabel("Modbus 开关量")
        coil_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        coil_layout.addWidget(coil_title)

        self.coil_checks = {}
        definitions = [("系统使能", "system_enable", 0), ("风机使能", "fan_enable", 2)]
        for title, name, address in definitions:
            row = QHBoxLayout()
            row.setSpacing(12)
            check = QCheckBox(title)
            btn = QPushButton("写入")
            btn.setObjectName("writeBtn")
            btn.clicked.connect(
                lambda _=False, a=address, c=check: self._safe_submit(
                    "write_coil", address=a, enabled=c.isChecked()
                )
            )
            self.coil_checks[name] = check
            addr_label = QLabel(f"0x{address:04X}")
            addr_label.setStyleSheet("font-size:12px;color:#9CA3AF;")
            row.addWidget(check)
            row.addWidget(addr_label)
            row.addWidget(btn)
            row.addStretch()
            coil_layout.addLayout(row)

        wrap.addWidget(coil_card)

        aa_card = QFrame()
        aa_card.setObjectName("card")
        aa_layout = QVBoxLayout(aa_card)
        aa_layout.setContentsMargins(20, 16, 20, 16)
        aa_layout.setSpacing(10)
        aa_title = QLabel("AA55 协议调试")
        aa_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        aa_layout.addWidget(aa_title)

        btn_row = QHBoxLayout()
        btn_row.setSpacing(8)
        self.ping_button = QPushButton("测试连接")
        self.ping_button.setObjectName("secondaryBtn")
        self.ping_button.clicked.connect(self._on_ping)
        self.version_button = QPushButton("获取版本号")
        self.version_button.setObjectName("secondaryBtn")
        self.version_button.clicked.connect(self._on_get_version)
        self.led_button = QPushButton("打开LED")
        self.led_button.setObjectName("secondaryBtn")
        self.led_button.clicked.connect(self._on_toggle_led)
        self.store_cfg_button = QPushButton("保存配置")
        self.store_cfg_button.setObjectName("warningBtn")
        self.store_cfg_button.clicked.connect(lambda: self._safe_submit("store_config"))
        self.load_cfg_button = QPushButton("加载配置")
        self.load_cfg_button.setObjectName("warningBtn")
        self.load_cfg_button.clicked.connect(lambda: self._safe_submit("load_config"))
        self.defaults_button = QPushButton("恢复默认")
        self.defaults_button.setObjectName("warningBtn")
        self.defaults_button.clicked.connect(lambda: self._safe_submit("restore_defaults"))

        btn_row.addWidget(self.ping_button)
        btn_row.addWidget(self.version_button)
        btn_row.addWidget(self.led_button)
        btn_row.addWidget(self.store_cfg_button)
        btn_row.addWidget(self.load_cfg_button)
        btn_row.addWidget(self.defaults_button)
        btn_row.addStretch()
        aa_layout.addLayout(btn_row)

        status_row = QHBoxLayout()
        status_row.setSpacing(24)
        self.connection_status_label = QLabel("连接状态：未测试")
        self.connection_status_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.version_label = QLabel("设备版本：--")
        self.version_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.led_status_label = QLabel("LED状态：未知")
        self.led_status_label.setStyleSheet("font-size:12px;color:#6B7280;")
        status_row.addWidget(self.connection_status_label)
        status_row.addWidget(self.version_label)
        status_row.addWidget(self.led_status_label)
        status_row.addStretch()
        aa_layout.addLayout(status_row)

        wrap.addWidget(aa_card)

        pwm_card = QFrame()
        pwm_card.setObjectName("card")
        pwm_layout = QVBoxLayout(pwm_card)
        pwm_layout.setContentsMargins(20, 16, 20, 16)
        pwm_layout.setSpacing(10)
        pwm_title = QLabel("执行器参数（Modbus 保持寄存器）")
        pwm_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        pwm_layout.addWidget(pwm_title)

        for title, address, maximum in (("风机PWM（‰）", 3, 1000),):
            row = QHBoxLayout()
            row.setSpacing(12)
            lbl = QLabel(title)
            lbl.setStyleSheet("font-size:12px;color:#6B7280;")
            spin = QSpinBox()
            spin.setRange(0, maximum)
            btn = QPushButton("写入")
            btn.setObjectName("writeBtn")
            btn.clicked.connect(
                lambda _=False, a=address, s=spin: self._safe_submit(
                    "write_register", address=a, value=s.value()
                )
            )
            row.addWidget(lbl)
            row.addWidget(spin)
            row.addWidget(btn)
            row.addStretch()
            pwm_layout.addLayout(row)

        wrap.addWidget(pwm_card)

        hp_card = QFrame()
        hp_card.setObjectName("card")
        hp_layout = QVBoxLayout(hp_card)
        hp_layout.setContentsMargins(20, 16, 20, 16)
        hp_layout.setSpacing(10)
        hp_title = QLabel("高压保护 + 压缩机调试")
        hp_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        hp_layout.addWidget(hp_title)

        hp_grid = QGridLayout()
        hp_grid.setSpacing(12)

        self.compressor_button = QPushButton("启动压缩机(调试)")
        self.compressor_button.setObjectName("dangerBtn")
        self.compressor_button.setCheckable(True)
        self.compressor_button.clicked.connect(self._on_toggle_compressor)

        self.hp_status_label = QLabel("高压：--")
        self.hp_status_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.hp_lock_label = QLabel("锁机：--")
        self.hp_lock_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.hp_count_label = QLabel("次数：--")
        self.hp_count_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.hp_timer_label = QLabel("计时：--")
        self.hp_timer_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.k0_state_label = QLabel("K0开关：--")
        self.k0_state_label.setStyleSheet("font-size:12px;color:#6B7280;")

        hp_grid.addWidget(self.compressor_button, 0, 0)
        hp_grid.addWidget(self.k0_state_label, 0, 1)
        hp_grid.addWidget(self.hp_status_label, 1, 0)
        hp_grid.addWidget(self.hp_lock_label, 1, 1)
        hp_grid.addWidget(self.hp_count_label, 2, 0)
        hp_grid.addWidget(self.hp_timer_label, 2, 1)

        hp_layout.addLayout(hp_grid)
        wrap.addWidget(hp_card)
        wrap.addStretch()
        return page

    def _parameters_page(self) -> QWidget:
        page = QWidget()
        wrap = QVBoxLayout(page)
        wrap.setContentsMargins(0, 0, 0, 0)
        wrap.setSpacing(12)

        table_card = QFrame()
        table_card.setObjectName("card")
        table_layout = QVBoxLayout(table_card)
        table_layout.setContentsMargins(16, 16, 16, 16)
        table_layout.setSpacing(12)

        table_title = QLabel("保持寄存器参数")
        table_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        table_layout.addWidget(table_title)

        self.param_table = QTableWidget(0, 7)
        self.param_table.setHorizontalHeaderLabels(
            ["名称", "地址", "当前值", "单位", "新值", "范围", "写入"]
        )
        self.param_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        items = self.register_map.items("holding_registers")
        self.param_table.setRowCount(len(items))
        self.param_rows = {}

        for row, item in enumerate(items):
            self.param_rows[item.name] = row
            self.param_table.setItem(row, 0, QTableWidgetItem(item.label))
            self.param_table.setItem(row, 1, QTableWidgetItem(f"0x{item.address:04X}"))
            self.param_table.setItem(row, 2, QTableWidgetItem("--"))
            self.param_table.setItem(row, 3, QTableWidgetItem(item.unit))
            edit = QDoubleSpinBox()
            edit.setDecimals(3)
            edit.setRange(
                item.minimum if item.minimum is not None else -65535,
                item.maximum if item.maximum is not None else 65535,
            )
            self.param_table.setCellWidget(row, 4, edit)
            self.param_table.setItem(row, 5, QTableWidgetItem(f"{item.minimum}～{item.maximum}"))
            btn = QPushButton("写入")
            btn.setObjectName("writeBtn")
            btn.setEnabled(item.writable)
            btn.clicked.connect(
                lambda _=False, it=item, ed=edit: self._write_mapped(it, ed)
            )
            self.param_table.setCellWidget(row, 6, btn)

        table_layout.addWidget(self.param_table)

        save = QPushButton("保存参数到EEPROM")
        save.setObjectName("warningBtn")
        save.clicked.connect(lambda: self._safe_submit("store_config"))
        table_layout.addWidget(save)

        wrap.addWidget(table_card)

        eeprom_card = QFrame()
        eeprom_card.setObjectName("card")
        eeprom_layout = QVBoxLayout(eeprom_card)
        eeprom_layout.setContentsMargins(20, 16, 20, 16)
        eeprom_layout.setSpacing(12)

        eeprom_title = QLabel("EEPROM 持久化测试")
        eeprom_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        eeprom_layout.addWidget(eeprom_title)

        test_row = QHBoxLayout()
        test_row.setSpacing(12)
        test_label = QLabel("测试值")
        test_label.setStyleSheet("font-size:12px;color:#6B7280;")
        self.eeprom_test_value = QSpinBox()
        self.eeprom_test_value.setRange(0, 1000)
        self.eeprom_test_value.setValue(600)
        self.eeprom_write_save_button = QPushButton("写寄存器3并保存")
        self.eeprom_write_save_button.setObjectName("writeBtn")
        self.eeprom_write_save_button.clicked.connect(
            lambda: self._safe_submit(
                "eeprom_test_write_save", address=3, value=self.eeprom_test_value.value()
            )
        )
        self.eeprom_verify_button = QPushButton("重启后验证寄存器3")
        self.eeprom_verify_button.setObjectName("secondaryBtn")
        self.eeprom_verify_button.clicked.connect(
            lambda: self._safe_submit(
                "eeprom_verify", address=3, expected=self.eeprom_test_value.value()
            )
        )
        self.eeprom_status_label = QLabel("EEPROM测试：未执行")
        self.eeprom_status_label.setStyleSheet("font-size:12px;color:#6B7280;")

        test_row.addWidget(test_label)
        test_row.addWidget(self.eeprom_test_value)
        test_row.addWidget(self.eeprom_write_save_button)
        test_row.addWidget(self.eeprom_verify_button)
        test_row.addWidget(self.eeprom_status_label)
        test_row.addStretch()

        eeprom_layout.addLayout(test_row)
        wrap.addWidget(eeprom_card)
        wrap.addStretch()
        return page

    def _manual_page(self) -> QWidget:
        page = QWidget()
        wrap = QVBoxLayout(page)
        wrap.setContentsMargins(0, 0, 0, 0)
        wrap.setSpacing(12)

        card = QFrame()
        card.setObjectName("card")
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(20, 16, 20, 16)
        card_layout.setSpacing(14)

        card_title = QLabel("手动读写操作")
        card_title.setStyleSheet("font-size:14px;font-weight:600;color:#1F2937;")
        card_layout.addWidget(card_title)

        form = QFormLayout()
        form.setSpacing(10)

        self.manual_op = QComboBox()
        self.manual_op.addItems([
            "PING", "GET_VERSION", "ECHO", "GET_STATUS",
            "01 读线圈", "02 读离散输入", "03 读保持寄存器",
            "04 读输入寄存器", "05 写单线圈", "06 写单寄存器", "10 写多个寄存器",
        ])
        self.manual_addr = QSpinBox()
        self.manual_addr.setRange(0, 65535)
        self.manual_count = QSpinBox()
        self.manual_count.setRange(1, 125)
        self.manual_data = QLineEdit()
        self.manual_data.setPlaceholderText("ECHO填HEX；写寄存器填数值或逗号分隔列表")

        btn = QPushButton("执行")
        btn.setObjectName("primaryBtn")
        btn.clicked.connect(
            lambda: self._safe_submit(
                "manual",
                operation=self.manual_op.currentText(),
                address=self.manual_addr.value(),
                count=self.manual_count.value(),
                data=self.manual_data.text(),
            )
        )

        self.manual_result = QPlainTextEdit()
        self.manual_result.setReadOnly(True)
        self.manual_result.setMaximumHeight(300)
        self.manual_result.setStyleSheet(
            "background:#F9FAFB;border:1px solid #E5E7EB;border-radius:6px;"
            "font-size:12px;padding:10px;"
        )

        form.addRow("功能", self.manual_op)
        form.addRow("地址", self.manual_addr)
        form.addRow("数量", self.manual_count)
        form.addRow("数据", self.manual_data)
        form.addRow(btn)
        form.addRow("结果", self.manual_result)

        card_layout.addLayout(form)
        wrap.addWidget(card)
        wrap.addStretch()
        return page

    def _logs_page(self) -> QWidget:
        page = QWidget()
        wrap = QVBoxLayout(page)
        wrap.setContentsMargins(0, 0, 0, 0)
        wrap.setSpacing(12)

        card = QFrame()
        card.setObjectName("card")
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(16, 16, 16, 16)
        card_layout.setSpacing(10)

        top_row = QHBoxLayout()
        top_row.setSpacing(8)

        clear = QPushButton("清空")
        clear.setObjectName("secondaryBtn")
        clear.clicked.connect(lambda: self.log_text.clear())

        save = QPushButton("导出日志")
        save.setObjectName("secondaryBtn")
        save.clicked.connect(self.save_log)

        self.auto_scroll = QCheckBox("自动滚动")
        self.auto_scroll.setChecked(True)

        self.stats = QLabel("TX 0 | RX 0 | 错误 0 | 轮询 0")
        self.stats.setStyleSheet("font-size:12px;color:#6B7280;")

        top_row.addWidget(clear)
        top_row.addWidget(save)
        top_row.addWidget(self.auto_scroll)
        top_row.addStretch()
        top_row.addWidget(self.stats)
        card_layout.addLayout(top_row)

        self.log_text = QPlainTextEdit()
        self.log_text.setObjectName("logArea")
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumBlockCount(10000)

        card_layout.addWidget(self.log_text, 1)
        wrap.addWidget(card, 1)
        return page

    # ==================== Signal Wiring ====================

    def _connect_signals(self) -> None:
        self.refresh_button.clicked.connect(self.refresh_ports)
        self.connect_button.clicked.connect(self.toggle_connection)
        self.poll_checkbox.toggled.connect(
            lambda v: self.worker.submit("set_polling", enabled=v)
        )
        self.poll_spin.valueChanged.connect(
            lambda v: self.worker.submit("set_poll_interval", milliseconds=v)
        )
        self.worker.connected.connect(self.on_connected)
        self.worker.disconnected.connect(self.on_disconnected)
        self.worker.status_updated.connect(self.on_status)
        self.worker.command_finished.connect(self.on_command)
        self.worker.communication_error.connect(self.on_error)
        self.worker.raw_log.connect(self.on_raw)
        self.worker.statistics_updated.connect(self.on_stats)

    # ==================== Business Logic (preserved) ====================

    def refresh_ports(self) -> None:
        current = self.port_combo.currentData()
        self.port_combo.clear()
        for port in sorted(list_ports.comports(), key=lambda x: x.device):
            self.port_combo.addItem(f"{port.device} — {port.description}", port.device)
        if current:
            index = self.port_combo.findData(current)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def toggle_connection(self) -> None:
        if self.connected:
            self.worker.submit("disconnect")
            return
        port = self.port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "提示", "没有选择可用串口")
            return
        self.connect_button.setEnabled(False)
        self.worker.submit(
            "connect",
            port=port,
            baudrate=int(self.baud_combo.currentText()),
            parity=self.parity_combo.currentData(),
            address=self.address_spin.value(),
            protocol=self.protocol_combo.currentData(),
            timeout=0.3,
        )

    def on_connected(self, info: dict) -> None:
        self.connected = True
        self._set_connected(True)
        ver = info.get("version")
        extra = f"，版本{ver}" if ver else ""
        self.statusBar().showMessage(
            f"已连接{info['port']}，协议{info['protocol']}{extra}"
        )
        self.connection_status_label.setText("连接状态：已连接")
        self.header_port_label.setText(
            f"  {info.get('port', '')} · {info.get('protocol', '')}"
        )

    def on_disconnected(self) -> None:
        self.connected = False
        self._set_connected(False)
        self.statusBar().showMessage("已断开")
        self.header_port_label.setText("")

    def _set_connected(self, connected: bool) -> None:
        self.connect_button.setEnabled(True)
        self.connect_button.setText("断开" if connected else "连接")
        if connected:
            self.connect_button.setObjectName("dangerBtn")
        else:
            self.connect_button.setObjectName("primaryBtn")
        self.connect_button.style().unpolish(self.connect_button)
        self.connect_button.style().polish(self.connect_button)

        self.connection_label.setText("● 已连接" if connected else "● 未连接")
        self.connection_label.setStyleSheet(
            "font-size:13px;color:#22A06B;font-weight:600;"
            if connected
            else "font-size:13px;color:#D64545;font-weight:600;"
        )

        self.header_status_badge.setText("● 已连接" if connected else "● 设备未连接")
        self.header_status_badge.setProperty(
            "status", "connected" if connected else "disconnected"
        )
        self._refresh_header_status()

        for widget in (
            self.protocol_combo,
            self.port_combo,
            self.baud_combo,
            self.parity_combo,
            self.address_spin,
            self.refresh_button,
        ):
            widget.setEnabled(not connected)

    def _refresh_header_status(self) -> None:
        self.header_status_badge.style().unpolish(self.header_status_badge)
        self.header_status_badge.style().polish(self.header_status_badge)
        st = self.header_status_badge.property("status")
        if st == "connected":
            self.header_status_badge.setStyleSheet(
                "font-size:13px;color:#22A06B;font-weight:600;"
            )
        else:
            self.header_status_badge.setStyleSheet(
                "font-size:13px;color:#D64545;font-weight:600;"
            )

    def on_status(self, state: dict) -> None:
        self.last_state.update(state)
        self._last_update = datetime.now()

        for name, card in self.cards.items():
            if name in state:
                if name == "fault_code":
                    val = state[name]
                    has_fault = val is not None and (
                        isinstance(val, bool)
                        or (isinstance(val, (int, float)) and val != 0)
                    )
                    card.set_value(val, "#D64545" if has_fault else "#22A06B")
                elif name == "device_status":
                    pass
                else:
                    card.set_value(state[name])

        if "device_status" in self.cards and (
            "system_enable" in state or "led" in state
        ):
            enabled = bool(self.last_state.get("system_enable", False))
            led = bool(self.last_state.get("led", False))
            parts = ["运行中"]
            if enabled and led:
                parts.append("LED开")
            elif not enabled:
                parts = ["已停止"]
            text = ", ".join(parts)
            color = "#22A06B" if enabled else "#6B7280"
            self.cards["device_status"].set_value(text, color)

        for name, check in self.coil_checks.items():
            if name in state:
                check.setChecked(bool(state[name]))

        if "led" in state:
            led_state = bool(state["led"])
            self.led_state = led_state
            self.led_status_label.setText(
                f"LED状态：{'已打开' if led_state else '已关闭'}"
            )
            if not self.led_running:
                self.led_button.setText("关闭LED" if led_state else "打开LED")

        for name, row in self.param_rows.items():
            if name in state:
                self.param_table.item(row, 2).setText(str(state[name]))

        if "hp_fault_code" in state:
            self._update_high_pressure_panel(state)

        if "K0" in state:
            hp_active = bool(
                int(
                    state.get("compressor_running", state.get("compressor_debug", 0))
                )
                or int(state.get("hp_fault_code", 0))
                or int(state.get("hp_locked", 0))
            )
            if hp_active:
                k0 = bool(state["K0"])
                self.k0_state_label.setText(f"K0：{'闭合' if k0 else '断开'}")
                self.k0_state_label.setStyleSheet(
                    "font-size:12px;color:#22A06B;" if k0 else "font-size:12px;color:#D64545;"
                )
            else:
                self.k0_state_label.setText("K0：待机未检测")
                self.k0_state_label.setStyleSheet("font-size:12px;color:#6B7280;")

        if "compressor_debug" in state:
            running = bool(state["compressor_debug"])
            self.compressor_button.setChecked(running)
            self.compressor_button.setText(
                "停止压缩机" if running else "启动压缩机(调试)"
            )

        self.sample += 1
        if "ambient_temp" in state or "target_temp" in state:
            self.x.append(self.sample)
            self.ambient_temp.append(
                float(state.get("ambient_temp", float("nan")))
            )
            self.target_temp.append(
                float(state.get("target_temp", float("nan")))
            )
            self.curve_ambient.setData(list(self.x), list(self.ambient_temp))
            self.curve_target.setData(list(self.x), list(self.target_temp))

        if self.logger.active:
            self.logger.write(self.last_state)

        self._update_dash_status(state)

    def _update_dash_status(self, state: dict) -> None:
        if not hasattr(self, "dash_switch_status") or self.dash_switch_status is None:
            return

        if "system_enable" in state:
            enabled = bool(state["system_enable"])
            self.dash_switch_status.setText("开启" if enabled else "关闭")
            self.dash_switch_status.setStyleSheet(
                f"font-size:12px;color:{'#22A06B' if enabled else '#6B7280'};font-weight:500;"
            )

        if "fan_enable" in state:
            fan = bool(state["fan_enable"])
            self.dash_fan_status.setText("运行中" if fan else "已停止")
            self.dash_fan_status.setStyleSheet(
                f"font-size:12px;color:{'#22A06B' if fan else '#6B7280'};font-weight:500;"
            )

        if "fault_code" in state:
            fc = state["fault_code"]
            has_fault = fc is not None and (
                isinstance(fc, bool)
                or (isinstance(fc, (int, float)) and fc != 0)
            )
            self.dash_fault_status.setText(
                "正常" if not has_fault else f"故障 ({fc})"
            )
            self.dash_fault_status.setStyleSheet(
                f"font-size:12px;color:{'#22A06B' if not has_fault else '#D64545'};font-weight:500;"
            )

        if self._last_update:
            self.dash_last_time.setText(self._last_update.strftime("%H:%M:%S"))

        self.dash_poll_status.setText(
            "轮询中" if self.poll_checkbox.isChecked() else "已停止"
        )

    def on_command(self, name: str, result) -> None:
        if name == "_connect_failed":
            self.connect_button.setEnabled(True)
            return
        if name in ("eeprom_test_write_save", "eeprom_verify"):
            self._handle_eeprom_result(name, result)
            return
        if name == "set_led" and not isinstance(result, dict):
            self.led_button.setEnabled(True)
            self.led_running = False
            self.led_state = bool(result)
            self.led_button.setText("关闭LED" if self.led_state else "打开LED")
            self.led_status_label.setText(
                f"LED状态：{'已打开' if self.led_state else '已关闭'}"
            )
            self.statusBar().showMessage(
                f"LED{'打开' if self.led_state else '关闭'}成功", 5000
            )
            return
        if isinstance(result, dict):
            self._handle_aa55_result(name, result)
        else:
            self.statusBar().showMessage(f"{name}执行成功：{result}", 5000)
        if name == "manual":
            self.manual_result.appendPlainText(
                json.dumps(result, ensure_ascii=False, indent=2, default=str)
            )

    def on_error(self, message: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.log_text.appendPlainText(f"{stamp} ERROR {message}")
        self.statusBar().showMessage(message, 8000)

    def on_raw(self, direction: str, text: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.log_text.appendPlainText(f"{stamp} {direction:<2} {text}")
        if self.auto_scroll.isChecked():
            self.log_text.verticalScrollBar().setValue(
                self.log_text.verticalScrollBar().maximum()
            )

    def on_stats(self, s: dict) -> None:
        self.stats.setText(
            f"TX {s['tx_frames']} | RX {s['rx_frames']} | 错误 {s['errors']} | 轮询 {s['polls']}"
        )

    def _require_connected(self) -> bool:
        if not self.connected:
            QMessageBox.warning(self, "提示", "请先打开串口连接设备")
            return False
        return True

    def _safe_submit(self, action, **payload):
        if self._require_connected():
            self.worker.submit(action, **payload)

    def _on_ping(self) -> None:
        if not self._require_connected():
            return
        self.ping_button.setEnabled(False)
        self.worker.submit("ping")

    def _on_get_version(self) -> None:
        if not self._require_connected():
            return
        self.version_button.setEnabled(False)
        self.worker.submit("get_version")

    def _on_toggle_led(self) -> None:
        if not self._require_connected():
            return
        self.led_button.setEnabled(False)
        self.led_running = True
        self.worker.submit("set_led", enabled=not self.led_state)

    def _on_toggle_compressor(self) -> None:
        if not self._require_connected():
            return
        on = self.compressor_button.isChecked()
        self.worker.submit("write_coil", address=7, enabled=on)
        self._compressor_on = on
        if on:
            self.hp_status_label.setText("高压：压缩机已启动，等待状态刷新")
            self.hp_status_label.setStyleSheet("color:#22A06B;")
            self.hp_timer_label.setText("运行计时：0s")
        else:
            self.hp_status_label.setText("高压：待机")
            self.hp_status_label.setStyleSheet("color:#6B7280;")
            self.hp_timer_label.setText("计时：--")

    def _update_high_pressure_panel(self, state: dict) -> None:
        fc = int(state.get("hp_fault_code", 0))
        locked = int(state.get("hp_locked", 0))
        count = int(state.get("hp_fault_count", 0))
        remain = int(state.get("hp_remain_sec", 0))
        running = int(state.get("compressor_running", state.get("compressor_debug", 0)))
        switch_closed = int(state.get("hp_switch_closed", state.get("K0", 0)))
        run_elapsed = int(state.get("compressor_run_elapsed", 0))
        protection_elapsed = int(state.get("hp_fault_elapsed", 0))

        in_protection = bool(locked or fc != 0)
        if not running and not in_protection:
            self.hp_status_label.setText("高压：待机")
            self.hp_status_label.setStyleSheet("color:#6B7280;")
            self.hp_lock_label.setText(f"锁机：{'是' if locked else '否'}")
            self.hp_count_label.setText(f"次数：{count}/3")
            self.hp_timer_label.setText(f"故障码：{fc}")
            return

        if locked:
            self.hp_status_label.setText(
                f"高压：锁机保护中，已进入保护时间：{protection_elapsed}s，剩余：{remain}s"
            )
            self.hp_status_label.setStyleSheet("color:#D64545;font-weight:bold;")
            self.hp_timer_label.setText(f"故障码：{fc}，保护计时：{protection_elapsed}s")
        elif in_protection:
            self.hp_status_label.setText(
                f"高压：已进入停机保护时间：{protection_elapsed}s"
            )
            self.hp_status_label.setStyleSheet("color:#D97706;font-weight:bold;")
            self.hp_timer_label.setText(f"故障码：{fc}，保护计时：{protection_elapsed}s")
        elif running:
            if switch_closed:
                self.hp_status_label.setText(
                    f"高压：正常，压缩机已经运行时间：{run_elapsed}s"
                )
                self.hp_status_label.setStyleSheet("color:#22A06B;")
            else:
                self.hp_status_label.setText(
                    f"高压：开关断开确认中，压缩机已经运行时间：{run_elapsed}s，距保护约：{remain}s"
                )
                self.hp_status_label.setStyleSheet("color:#D97706;font-weight:bold;")
            self.hp_timer_label.setText(f"运行计时：{run_elapsed}s")
        else:
            self.hp_status_label.setText("高压：压缩机已停止")
            self.hp_status_label.setStyleSheet("color:#6B7280;")
            self.hp_timer_label.setText(f"故障码：{fc}")

        self.hp_lock_label.setText(f"锁机：{'是' if locked else '否'}")
        self.hp_count_label.setText(f"次数：{count}/3")

    def _handle_aa55_result(self, name: str, result: dict) -> None:
        success = result["success"]
        elapsed = result.get("elapsed_ms", 0)
        if name == "ping":
            self.ping_button.setEnabled(True)
            if success:
                self.connection_status_label.setText("连接状态：连接正常")
                self.statusBar().showMessage(
                    f"连接测试成功，耗时 {elapsed:.0f} ms", 5000
                )
            else:
                self.connection_status_label.setText("连接状态：失败")
                self.statusBar().showMessage(
                    f"连接测试失败：{result['error']}", 5000
                )
        elif name == "get_version":
            self.version_button.setEnabled(True)
            if success:
                ver = result["data"]
                self.version_label.setText(f"设备版本：{ver}")
                self.statusBar().showMessage(
                    f"获取版本成功：{ver}，耗时 {elapsed:.0f} ms", 5000
                )
            else:
                self.statusBar().showMessage(
                    f"获取版本失败：{result['error']}", 5000
                )
        elif name == "set_led":
            self.led_button.setEnabled(True)
            self.led_running = False
            if success:
                self.led_state = result["data"]
                self.led_button.setText("关闭LED" if self.led_state else "打开LED")
                self.led_status_label.setText(
                    f"LED状态：{'已打开' if self.led_state else '已关闭'}"
                )
                self.statusBar().showMessage(
                    f"LED{'打开' if self.led_state else '关闭'}成功，耗时 {elapsed:.0f} ms",
                    5000,
                )
            else:
                self.statusBar().showMessage(
                    f"LED操作失败：{result['error']}", 5000
                )
        elif name in ("store_config", "load_config", "restore_defaults"):
            if success:
                state = result["data"]
                cmd_text = {
                    "store_config": "保存配置",
                    "load_config": "加载配置",
                    "restore_defaults": "恢复默认",
                }
                if isinstance(state, dict):
                    if state.get("saved"):
                        state_text = "已发送保存命令"
                    else:
                        state_text = json.dumps(state, ensure_ascii=False, default=str)
                else:
                    state_text = {0: "空闲", 1: "待保存", 2: "保存中", 3: "错误"}.get(
                        state, f"未知({state})"
                    )
                self.statusBar().showMessage(
                    f"{cmd_text.get(name, name)}完成，状态：{state_text}，耗时 {elapsed:.0f} ms",
                    5000,
                )
            else:
                self.statusBar().showMessage(f"{name}失败：{result['error']}", 5000)
        elif name == "get_config_status":
            if success:
                d = result["data"]
                src = {0: "空白", 1: "EEPROM离线", 2: "CRC错误", 3: "已加载"}.get(
                    d.get("config_source", 0), "?"
                )
                self.statusBar().showMessage(
                    f"EEPROM:{'在线' if d.get('eeprom_online') else '离线'} "
                    f"槽:{d.get('active_slot')} 状态:{d.get('state')} "
                    f"Seq:{d.get('sequence')} 保存:{d.get('save_count')}次 "
                    f"来源:{src} 耗时{elapsed:.0f}ms",
                    8000,
                )
            else:
                self.statusBar().showMessage(f"{name}失败：{result['error']}", 5000)

    def _handle_eeprom_result(self, name: str, result: dict) -> None:
        elapsed = result.get("elapsed_ms", 0) if isinstance(result, dict) else 0
        if not isinstance(result, dict) or not result.get("success"):
            error = (
                result.get("error", "验证失败")
                if isinstance(result, dict)
                else "验证失败"
            )
            text = f"EEPROM测试失败：{error}"
            self.eeprom_status_label.setText(text)
            self.statusBar().showMessage(text, 8000)
            return

        if name == "eeprom_test_write_save":
            value = result.get("value")
            readback = result.get("readback")
            text = f"已写入并保存：寄存器3={value}，回读={readback}，请断电/复位后点验证"
            self.eeprom_status_label.setText(text)
            self.statusBar().showMessage(f"{text}，耗时 {elapsed:.0f} ms", 10000)
        elif name == "eeprom_verify":
            expected = result.get("expected")
            value = result.get("value")
            text = f"EEPROM验证通过：期望={expected}，实际={value}"
            self.eeprom_status_label.setText(text)
            self.statusBar().showMessage(f"{text}，耗时 {elapsed:.0f} ms", 10000)

    def _write_mapped(self, item, edit: QDoubleSpinBox) -> None:
        try:
            raw = item.encode(edit.value())
        except ValueError as exc:
            QMessageBox.warning(self, "参数错误", str(exc))
            return
        self._safe_submit("write_register", address=item.address, value=raw)

    def save_log(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "导出通信日志", "communication.log", "日志文件 (*.log *.txt)"
        )
        if path:
            Path(path).write_text(self.log_text.toPlainText(), encoding="utf-8")

    def toggle_recording(self, checked: bool) -> None:
        if checked:
            default = datetime.now().strftime("airphone_%Y%m%d_%H%M%S.csv")
            path, _ = QFileDialog.getSaveFileName(
                self, "保存监控数据", default, "CSV文件 (*.csv)"
            )
            if not path:
                self.record_action.setChecked(False)
                return
            self.logger.start(path)
            self.record_action.setText("停止记录CSV")
        else:
            self.logger.stop()
            self.record_action.setText("开始记录CSV")

    def closeEvent(self, event: QCloseEvent) -> None:
        self.logger.stop()
        self.worker.stop()
        event.accept()
