GPU ?= 1
CONTAINERIZE ?= 1

CC := gcc
AR := ar
TOOLPREFIX :=
TOOLPOSTFIX :=

BIN := darkweb
PKG := github.com/squat/$(BIN)
DARKNETVERSION := master
MONGOOSEVERSION := 6.10
STBVERSION := master
BUILD_IMAGE := nvidia/cuda:9.1-cudnn7-devel
ifeq ($(CONTAINERIZE), 1)
TOOLPREFIX := docker run --rm -u $(shell id -u):$(shell id -g) -v $(shell pwd):/build -w /build $(BUILD_IMAGE) /bin/bash -c "
TOOLPOSTFIX := "
endif

REGISTRY ?= index.docker.io
IMAGE ?= squat/$(BIN)

TAG := $(shell git describe --abbrev=0 --tags HEAD 2>/dev/null)
COMMIT := $(shell git rev-parse HEAD)
VERSION := $(COMMIT)
ifneq ($(TAG),)
    ifeq ($(COMMIT), $(shell git rev-list -n1 $(TAG)))
        VERSION := $(TAG)
    endif
endif
DIRTY := $(shell test -z "$$(git diff --shortstat 2>/dev/null)" || echo -dirty)
VERSION := $(VERSION)$(DIRTY)

SRCDIR := ./src
BUILDDIR := ./build
OBJDIR := $(BUILDDIR)/obj
VENDORDIR := vendor
VPATH := $(SRCDIR):./$(VENDORDIR)
DARKNETDIR := $(VENDORDIR)/darknet

OBJ := detect.o handlers.o mongoose.o util.o darkweb.o libdarknet.a
OBJS := $(addprefix $(OBJDIR)/, $(OBJ))
DEPS := $(wildcard $(SRCDIR)/*.h) $(wildcard $(VENDORDIR)/*.h) Makefile
WEIGHT := yolov3.weights yolov2-tiny.weights yolo9000.weights
WEIGHTS := $(addprefix $(DARKNETDIR)/, $(WEIGHT))

CFLAGS := -DMG_ENABLE_CALLBACK_USERDATA -DMG_ENABLE_HTTP_STREAMING_MULTIPART -I$(SRCDIR) -I$(VENDORDIR) -I$(DARKNETDIR)/include
LDFLAGS :=
LDLIBS := -lm -lpthread
ifeq ($(GPU), 1)
LDFLAGS += -L/usr/local/cuda/lib64
LDLIBS += -lstdc++ -lcudart -lcublas -lcurand -lcuda -lcudnn
CFLAGS += -DGPU -I/usr/local/cuda/include/ -DCUDNN
endif

$(BIN): $(OBJS)
	$(TOOLPREFIX)$(CC) $^ $(LDFLAGS) -o $@ $(LDLIBS)$(TOOLPOSTFIX)

$(OBJDIR)/%.o: %.c $(DEPS)
	mkdir -p $(OBJDIR)
	$(TOOLPREFIX)$(CC) $(CFLAGS) -c $< -o $@$(TOOLPOSTFIX)

$(OBJDIR)/libdarknet.a: $(DARKNETDIR)/libdarknet.a
	cp $(DARKNETDIR)/libdarknet.a $(OBJDIR)/libdarknet.a

$(DARKNETDIR)/libdarknet.a:
	$(TOOLPREFIX)make -C $(DARKNETDIR) GPU=$(GPU) CUDNN=$(GPU) obj libdarknet.a$(TOOLPOSTFIX)

.PHONY: vendor
vendor:
	mkdir -p $(VENDORDIR)
	curl -L -o $(VENDORDIR)/stb_image.h https://raw.githubusercontent.com/nothings/stb/$(STBVERSION)/stb_image.h
	curl -L -o $(VENDORDIR)/mongoose.tar.gz https://github.com/cesanta/mongoose/archive/$(MONGOOSEVERSION).tar.gz
	tar xf $(VENDORDIR)/mongoose.tar.gz -C $(VENDORDIR) --strip-components=1 mongoose-$(MONGOOSEVERSION)/mongoose.c
	tar xf $(VENDORDIR)/mongoose.tar.gz -C $(VENDORDIR) --strip-components=1 mongoose-$(MONGOOSEVERSION)/mongoose.h
	rm $(VENDORDIR)/mongoose.tar.gz
	curl -L -o $(VENDORDIR)/darknet.tar.gz https://github.com/pjreddie/darknet/archive/$(DARKNETVERSION).tar.gz
	mkdir -p $(DARKNETDIR)
	tar xf $(VENDORDIR)/darknet.tar.gz -C $(DARKNETDIR) --strip-components=1
	rm $(VENDORDIR)/darknet.tar.gz
	sed -i 's/GPU=0/GPU ?= 0/g' $(DARKNETDIR)/Makefile
	sed -i 's/CUDNN=0/CUDNN ?= 0/g' $(DARKNETDIR)/Makefile
	sed -i 's|^cfg/||g' $(DARKNETDIR)/.gitignore
	sed -i 's|^data/||g' $(DARKNETDIR)/.gitignore
	grep -rl '\<data\/' $(DARKNETDIR)/cfg | xargs sed -i -e 's|\bdata/|$(DARKNETDIR)/data/|g'

.PHONY: weights
weights: $(WEIGHTS)

$(DARKNETDIR)/%.weights:
	mkdir -p $(DARKNETDIR)
	curl -L -o $@ https://pjreddie.com/media/files/$(@F)

.PHONY: format
format:
	clang-format -i -style=file $(SRCDIR)/*

.PHONY: clean
clean: container-clean
	rm -rf $(DARKNETDIR)/obj $(DARKNETDIR)/libdarknet.a $(OBJDIR) $(BIN)

.PHONY: container
container: .container-$(VERSION) container-name
.container-$(VERSION): $(BIN) Dockerfile $(WEIGHTS)
	@docker build -t $(IMAGE):$(VERSION) .
	@docker images -q $(IMAGE):$(VERSION) > $@

.PHONY: container-latest
container-latest: .container-$(VERSION)
	@docker tag $(IMAGE):$(VERSION) $(IMAGE):latest
	@echo "container: $(IMAGE):latest"

.PHONY: container-name
container-name:
	@echo "container: $(IMAGE):$(VERSION)"

.PHONY: push
push: .push-$(VERSION) push-name
.push-$(VERSION): .container-$(VERSION)
	@docker push $(REGISTRY)/$(IMAGE):$(VERSION)
	@docker images -q $(IMAGE):$(VERSION) > $@

.PHONY: push-latest
push-latest: container-latest
	@docker push $(REGISTRY)/$(IMAGE):latest
	@echo "pushed: $(IMAGE):latest"

.PHONY: push-name
push-name:
	@echo "pushed: $(IMAGE):$(VERSION)"

.PHONY: container-clean
container-clean:
	rm -rf .container-* .push-*

