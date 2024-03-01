# Docker tips

- docker ps (view docker process)
- docker images (view docker image-list)
- (build) docker image build -t ubuntu:22.04 .
- (execute (first time)) docker run -d --name gcb_analyzer -p 8080:8080 -it ubuntu:22.04
- docker kill ['container-proc-id' or 'container-name'] (kill docker process)
- docker ps -a (view docker containers)
- (execute) docker start ['container-proc-id' or 'container-name']
