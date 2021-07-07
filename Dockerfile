## Note that these IMG_* ARG values are defaults, but actual automated builds use
## values supplied by the build system
ARG IMG_USER=project8
ARG IMG_REPO=p8compute_dependencies
ARG IMG_TAG=v1.0.0

FROM ${IMG_USER}/${IMG_REPO}:${IMG_TAG} as sandfly_common

ARG build_type=Release
ENV SANDFLY_BUILD_TYPE=$build_type

ARG SANDFLY_TAG=beta
ENV SANDFLY_TAG=${SANDFLY_TAG}
ARG SANDFLY_BASE_PREFIX=/usr/local/p8/sandfly
ARG SANDFLY_BUILD_PREFIX=${SANDFLY_BASE_PREFIX}/${SANDFLY_PREFIX_TAG}
ENV SANDFLY_BUILD_PREFIX=${SANDFLY_BUILD_PREFIX}
ARG SANDFLY_BUILD_TYPE=RELEASE

ARG CC_VAL=gcc
ENV CC=${CC_VAL}
ARG CXX_VAL=g++
ENV CXX=${CXX_VAL}

RUN mkdir -p $SANDFLY_BUILD_PREFIX &&\
    chmod -R 777 $SANDFLY_BUILD_PREFIX/.. &&\
    cd $SANDFLY_BUILD_PREFIX &&\
    echo "source ${COMMON_BUILD_PREFIX}/setup.sh" > setup.sh &&\
    echo "export SANDFLY_TAG=${SANDFLY_TAG}" >> setup.sh &&\
    echo "export SANDFLY_BUILD_PREFIX=${SANDFLY_BUILD_PREFIX}" >> setup.sh &&\
    echo 'ln -sfT $SANDFLY_BUILD_PREFIX $SANDFLY_BUILD_PREFIX/../current' >> setup.sh &&\
    echo 'export PATH=$SANDFLY_BUILD_PREFIX/bin:$PATH' >> setup.sh &&\
    echo 'export LD_LIBRARY_PATH=$SANDFLY_BUILD_PREFIX/lib:$LD_LIBRARY_PATH' >> setup.sh &&\
    echo 'export LD_LIBRARY_PATH=$SANDFLY_BUILD_PREFIX/lib64:$LD_LIBRARY_PATH' >> setup.sh &&\
    /bin/true

########################
FROM sandfly_common as sandfly_done

COPY cmake /tmp_source/cmake
COPY dripline-cpp /tmp_source/dripline-cpp
COPY midge /tmp_source/midge
COPY library /tmp_source/library
COPY executables /tmp_source/executables
COPY CMakeLists.txt /tmp_source/CMakeLists.txt
COPY SandflyConfig.cmake.in /tmp_source/SandflyConfig.cmake.in
COPY .git /tmp_source/.git

## store cmake args because we'll need to run twice (known package_builder issue)
## use EXTRA_CMAKE_ARGS to add or replace options at build time, CMAKE_CONFIG_ARGS_LIST are defaults
ARG EXTRA_CMAKE_ARGS=""
ENV CMAKE_CONFIG_ARGS_LIST="\
      -D CMAKE_BUILD_TYPE=$SANDFLY_BUILD_TYPE \
      -D CMAKE_INSTALL_PREFIX:PATH=$SANDFLY_BUILD_PREFIX \
      ${EXTRA_CMAKE_ARGS} \
      ${OS_CMAKE_ARGS} \
      "

RUN source $SANDFLY_BUILD_PREFIX/setup.sh \
    && mkdir -p /tmp_source/build \
    && cd /tmp_source/build \
    && cmake ${CMAKE_CONFIG_ARGS_LIST} .. \
    && cmake ${CMAKE_CONFIG_ARGS_LIST} .. \
    && make install \
    && /bin/true

########################
FROM sandfly_common

# for now we must grab the extra dependency content as well as sandfly itself
COPY --from=sandfly_done $SANDFLY_BUILD_PREFIX $SANDFLY_BUILD_PREFIX

# for a sandfly container, we need the environment to be configured, this is not desired for compute containers
# (in which case the job should start with a bare shell and then do the exact setup desired)
ENTRYPOINT ["/bin/bash", "-l", "-c"]
CMD ["bash"]
RUN ln -s $SANDFLY_BUILD_PREFIX/setup.sh /etc/profile.d/setup.sh
