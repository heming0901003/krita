from PyQt5.QtWidgets import QWidget, QVBoxLayout, QToolBar, QTableWidget, QAction
from . import clearaction, outputtextedit


class OutPutWidget(QWidget):

    def __init__(self, scripter, parent=None):
        super(OutPutWidget, self).__init__(parent)

        self.scripter = scripter
        self.setObjectName('OutPut')
        self.layout = QVBoxLayout()

        self.toolbar = QToolBar()
        self.clearAction = clearaction.ClearAction(self.scripter, self)
        self.toolbar.addAction(self.clearAction)

        self.outputtextedit = outputtextedit.OutPutTextEdit(self.scripter, self)

        self.layout.addWidget(self.toolbar)
        self.layout.addWidget(self.outputtextedit)
        self.setLayout(self.layout)
