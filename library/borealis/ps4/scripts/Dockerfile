FROM ubuntu:24.04

MAINTAINER xfangfang <xfangfang@126.com>

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends ca-certificates curl wget git patch cmake make pkg-config zstd libarchive-tools libgpgme11 pacman-package-manager && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN echo "[pacbrew]" >> /etc/pacman.conf && \
    echo "SigLevel = Optional TrustAll" >> /etc/pacman.conf && \
    echo "Server = https://pacman.mydedibox.fr/pacbrew/packages/" >> /etc/pacman.conf && \
    ln -s /proc/mounts /etc/mtab && \
    pacman -Sy --noconfirm && \
    pacman -S --noconfirm ps4-openorbis pacbrew-ps4-portlibs && \
    yes | pacman -Scc && \
    cp /opt/pacbrew/ps4/openorbis/ps4vars.sh /root/.bash_aliases && \
    sed -i '306i #include <cstdlib>' /opt/pacbrew/ps4/openorbis/include/c++/v1/cmath

RUN mkdir /src/ &&\
    echo \#\!/bin/bash -i >> /entrypoint.sh &&\
    echo >> /entrypoint.sh &&\
    echo "set -e" >> /entrypoint.sh &&\
    echo "cd /src" >> /entrypoint.sh &&\
    echo "echo \"\$@\"" >> /entrypoint.sh &&\
    echo "bash -c \"\$@\"" >> /entrypoint.sh &&\
    chmod +x /entrypoint.sh

ENV LANG en_US.UTF-8
VOLUME /src/
WORKDIR /src/
SHELL ["/bin/bash", "-i", "-c"]
ENTRYPOINT ["/entrypoint.sh"]
