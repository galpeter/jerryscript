#### Apply patch to ESP8266 SDK

As `iram` is quite small to fit all the codes but linker tries to put it there.
To force JerryScript codes to be placed at the `irom` section,
need to change the order and tell the linker as below;

```
cd /opt/Espressif/ESP8266_SDK
- Create a patch.diff with the content below -
git apply patch.diff
```

```
diff --git a/ld/eagle.app.v6.common.ld b/ld/eagle.app.v6.common.ld
index 3af39d7..c395646 100644
--- a/ld/eagle.app.v6.common.ld
+++ b/ld/eagle.app.v6.common.ld
@@ -113,6 +113,12 @@ SECTIONS
     *(.irom0.literal .irom.literal .irom.text.literal .irom0.text .irom.text)
     *(.literal.* .text.*)
     _irom0_text_end = ABSOLUTE(.);
+
+    _jerry_text_start = ABSOLUTE(.);
+    *\libjerry-entry.a:*(.text*)
+    *\libjerry-core.a:*(.text*)
+    *\libjerry-libm.a:*(.text*)
+    _jerry_text_end = ABSOLUTE(.);
   } >irom0_0_seg :irom0_0_phdr

   .data : ALIGN(4)

```

#### Need to get setjmp / longjmp

Extract and copy from the SDK.

```
cd /path/to/jerryscript-folder
cd targets/esp8266/ && mkdir libs && cd libs
ar -xv $SDK_PATH/lib/libcirom.a lib_a-setjmp.o
```
