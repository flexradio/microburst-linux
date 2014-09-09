*** See the ARM infocenter technical support knowledge article on annotation:
http://infocenter.arm.com/help/topic/com.arm.doc.faqs/ka14989.html

*** Java/Android
StreamlineAnnotate.java provides the same functionality as the macros in streamline_annotate.h

*** Kernel/module annotation
Annotate from within the kernel or a module. The same macros are defined in streamline_annotate.h for both user-space and kernel-space using the _KERNEL_ preprocessor conditional

*** Annotate example
DS-5 includes Streamline_annotate, a simple linux application written in C which makes use of the text, marker, and visual annotate macros. To use the example, import into DS-5 Eclipse the Linux application example projects and refer to readme.html for more details.
