from PySide6.QtCore import Qt
from PySide6.QtWidgets import QFrame, QLabel, QVBoxLayout


class ValueCard(QFrame):
    def __init__(self, title: str, unit: str = "", parent=None) -> None:
        super().__init__(parent)
        self.unit = unit
        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.setMinimumSize(140, 90)
        layout = QVBoxLayout(self)
        title_label = QLabel(title)
        title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.value_label = QLabel("--")
        self.value_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.value_label.setStyleSheet("font-size:23px;font-weight:600;color:#1769aa;")
        layout.addWidget(title_label); layout.addWidget(self.value_label)

    def set_value(self, value) -> None:
        if value is None: text = "--"
        elif isinstance(value, bool): text = "开启" if value else "关闭"
        else: text = f"{value}{' ' + self.unit if self.unit else ''}"
        self.value_label.setText(text)
