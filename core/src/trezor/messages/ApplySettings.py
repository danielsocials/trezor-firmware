# Automatically generated by pb2py
# fmt: off
import protobuf as p

if __debug__:
    try:
        from typing import Dict, List  # noqa: F401
        from typing_extensions import Literal  # noqa: F401
    except ImportError:
        pass


class ApplySettings(p.MessageType):
    MESSAGE_WIRE_TYPE = 25

    def __init__(
        self,
        language: str = None,
        label: str = None,
        use_passphrase: bool = None,
        homescreen: bytes = None,
        auto_lock_delay_ms: int = None,
        display_rotation: int = None,
        passphrase_always_on_device: bool = None,
        use_fee_pay: int = None,
        use_ble: bool = None,
        use_se: bool = None,
        use_exportseeds: bool = None,
        is_bixinapp: bool = None,
    ) -> None:
        self.language = language
        self.label = label
        self.use_passphrase = use_passphrase
        self.homescreen = homescreen
        self.auto_lock_delay_ms = auto_lock_delay_ms
        self.display_rotation = display_rotation
        self.passphrase_always_on_device = passphrase_always_on_device
        self.use_fee_pay = use_fee_pay
        self.use_ble = use_ble
        self.use_se = use_se
        self.use_exportseeds = use_exportseeds
        self.is_bixinapp = is_bixinapp

    @classmethod
    def get_fields(cls) -> Dict:
        return {
            1: ('language', p.UnicodeType, 0),
            2: ('label', p.UnicodeType, 0),
            3: ('use_passphrase', p.BoolType, 0),
            4: ('homescreen', p.BytesType, 0),
            6: ('auto_lock_delay_ms', p.UVarintType, 0),
            7: ('display_rotation', p.UVarintType, 0),
            8: ('passphrase_always_on_device', p.BoolType, 0),
            9: ('use_fee_pay', p.UVarintType, 0),
            10: ('use_ble', p.BoolType, 0),
            11: ('use_se', p.BoolType, 0),
            12: ('use_exportseeds', p.BoolType, 0),
            13: ('is_bixinapp', p.BoolType, 0),
        }
