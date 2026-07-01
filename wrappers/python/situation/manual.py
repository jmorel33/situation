"""Hand-written wrappers for variadic / callback symbols omitted from foreign.py."""

from __future__ import annotations

from ctypes import c_char_p, c_int

from . import callbacks as CB

_log_callback_ref = None


def situation_log(dll, message: str) -> None:
    dll.SituationLog(c_char_p(message.encode("utf-8")))


def situation_log_warning(dll, message: str) -> None:
    dll.SituationLogWarning(c_char_p(message.encode("utf-8")))


def situation_image_draw_text_formatted(dll, image, font, x, y, size, color, fmt: str, *args) -> int:
    text = fmt % args if args else fmt
    return dll.SituationImageDrawText(
        image,
        font,
        c_char_p(text.encode("utf-8")),
        x,
        y,
        size,
        color,
    )


def situation_set_log_callback(dll, callback: CB.SituationLogCallback | None) -> None:
    global _log_callback_ref
    _log_callback_ref = callback
    dll.SituationSetLogCallback.argtypes = [CB.SituationLogCallback]
    dll.SituationSetLogCallback.restype = None
    dll.SituationSetLogCallback(callback)
