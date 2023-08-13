SPA_ANGULAR_APP_NAME = sample-angular-app
SPA_VUE_APP_NAME = sample-vue-app

all: angular

angular:
	@cd frontend/$(SPA_ANGULAR_APP_NAME) && npm install && npm run build
	@cp -r frontend/$(SPA_ANGULAR_APP_NAME)/dist/$(SPA_ANGULAR_APP_NAME)/* backend/romfs/webapp
	@cd backend && make

vue:
	@cd frontend/$(SPA_VUE_APP_NAME) && npm install && npm run build
	@cp -r frontend/$(SPA_VUE_APP_NAME)/dist/* backend/romfs/webapp
	@cd backend && make

clean:
	@cd frontend/$(SPA_ANGULAR_APP_NAME) && rm -rf dist
	@cd backend && rm -rf romfs/webapp/* && touch romfs/webapp/.gitkeep
	@cd backend && make clean

deploy:
	@cd backend && nxlink -s out/nx-spa.nro

.PHONY: clean all angular vue deploy