SPA_ANGULAR_APP_NAME = sample-angular-app
SPA_VUE_APP_NAME = sample-vue-app
WEBAPP_DIR = backend/romfs/webapp

all: angular

angular:
	@cd frontend/$(SPA_ANGULAR_APP_NAME) && npm ci --no-audit --no-fund && npm run build
	@rm -rf $(WEBAPP_DIR)
	@mkdir -p $(WEBAPP_DIR)
	@touch $(WEBAPP_DIR)/.gitkeep
	@cp -r frontend/$(SPA_ANGULAR_APP_NAME)/dist/$(SPA_ANGULAR_APP_NAME)/* $(WEBAPP_DIR)
	@cd backend && make

vue:
	@cd frontend/$(SPA_VUE_APP_NAME) && npm ci --no-audit --no-fund && npm run build
	@rm -rf $(WEBAPP_DIR)
	@mkdir -p $(WEBAPP_DIR)
	@touch $(WEBAPP_DIR)/.gitkeep
	@cp -r frontend/$(SPA_VUE_APP_NAME)/dist/* $(WEBAPP_DIR)
	@cd backend && make

clean:
	@rm -rf frontend/$(SPA_ANGULAR_APP_NAME)/dist frontend/$(SPA_VUE_APP_NAME)/dist
	@rm -rf $(WEBAPP_DIR)
	@mkdir -p $(WEBAPP_DIR)
	@touch $(WEBAPP_DIR)/.gitkeep
	@cd backend && make clean

deploy:
	@cd backend && nxlink -s out/nx-spa.nro

format:
	@cd backend && make format

.PHONY: clean all angular vue deploy format
