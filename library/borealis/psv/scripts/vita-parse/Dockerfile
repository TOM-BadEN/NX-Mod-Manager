FROM vitasdk/vitasdk:latest

ADD psv_debug.sh .

RUN apk update && \
    apk add python3 py3-pip && \
    wget https://github.com/xyzz/vita-parse-core/archive/refs/heads/master.zip && \
    unzip master.zip && \
    rm master.zip && \
    pip install -r vita-parse-core-master/requirements.txt --break-system-packages && \
    sed -i 's/from collections import MutableMapping/from collections.abc import MutableMapping/g' `python -c "import site; print(site.getsitepackages()[0])"`/elftools/construct/lib/container.py

WORKDIR /vita-parse-core-master
ENTRYPOINT ["/psv_debug.sh"]
