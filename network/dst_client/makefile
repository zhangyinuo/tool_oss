CC=gcc
CXX=g++
INC+= -I../ -I../../lib  -I../../3rdlib
LIB+=-lpthread  -L../../lib -lvfs   -L../../3rdlib -lnmshm -lstdc++
CFLAGS+= -g -O2 -pipe -Wall -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64 -fPIC -shared #-z defs
TARGET=dst_client.so
ALL:$(TARGET)

OBJ=vfs_data.o vfs_data_task.o

$(TARGET):$(OBJ)
	$(CC)	$(CFLAGS) -o $@ $^ $(LIB)
%.o: %.cpp
	$(CXX)  $(CFLAGS) $(INC) -c -o $@ $< 	 
%.o: %.c
	$(CC)  $(CFLAGS) $(INC) -c -o $@ $< 	 
clean:
	rm -f *.o *.so
	rm -f $(TARGET) 	
