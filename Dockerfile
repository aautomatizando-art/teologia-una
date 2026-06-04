FROM node:20-alpine
WORKDIR /app
COPY package.json .
RUN npm install
COPY encoder/relay/server.js encoder/relay/server.js
EXPOSE 3001
CMD ["node", "encoder/relay/server.js"]
