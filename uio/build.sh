if ! which lsuio; then
        (cd /home/root/tool && tar zxf lsuio-0.2.0.tar.gz && cd lsuio-0.2.0/ && ./configure >/dev/null && make -j4 && make install && lsuio)
fi
