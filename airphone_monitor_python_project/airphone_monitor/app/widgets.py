from PySide6.QtCore import Qt
from PySide6.QtWidgets import QFrame, QLabel, QVBoxLayout


class ValueCard(QFrame):
    def __init__(self, title: str, unit: str = "", value_color: str = "#2563EB", parent=None) -> None:
        super().__init__(parent)
        self.unit = unit
        self._value_color = value_color
        self.setObjectName("valueCard")
        self.setMinimumSize(150, 100)
        self.setMaximumHeight(136)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(6)

        self.title_label = QLabel(title)
        self.title_label.setObjectName("cardTitle")
        self.title_label.setAlignment(Qt.AlignmentFlag.AlignLeft)

        self.value_label = QLabel("--")
        self.value_label.setObjectName("cardValue")
        self.value_label.setAlignment(Qt.AlignmentFlag.AlignLeft)
        self.value_label.setStyleSheet(
            f"font-size:26px;font-weight:700;color:{value_color};"
        )

        self.detail_label = QLabel("")
        self.detail_label.setObjectName("cardDetail")
        self.detail_label.setAlignment(Qt.AlignmentFlag.AlignLeft)
        self.detail_label.setStyleSheet(
            "font-size:12px;font-weight:500;color:rgba(107, 114, 128, 150);"
        )

        layout.addWidget(self.title_label)
        layout.addWidget(self.value_label)
        layout.addWidget(self.detail_label)

    def set_value(self, value, color: str | None = None) -> None:
        if value is None:
            text = "--"
        elif isinstance(value, bool):
            text = "开启" if value else "关闭"
        else:
            text = f"{value}{' ' + self.unit if self.unit else ''}"
        self.value_label.setText(text)
        if color is not None:
            self.value_label.setStyleSheet(
                f"font-size:26px;font-weight:700;color:{color};"
            )

    def set_detail(self, text: str | None) -> None:
        self.detail_label.setText(text or "")
