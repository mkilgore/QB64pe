
libqb-objs-y += $(PATH_LIBQB)/src/main.o
libqb-objs-y += $(PATH_LIBQB)/src/threading.o
libqb-objs-y += $(PATH_LIBQB)/src/buffer.o

libqb-objs-$(DEP_HTTP) += $(PATH_LIBQB)/src/http.o

libqb-objs-y += $(PATH_LIBQB)/src/threading-$(PLATFORM).o

CLEAN_LIST += $(libqb-objs-y)
