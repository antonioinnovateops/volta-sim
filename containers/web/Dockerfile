# Stage 1: Build
FROM node:20-alpine AS builder

WORKDIR /app
COPY web/package.json web/package-lock.json* /app/
RUN npm install

COPY web/ /app/
RUN npm run build

# Stage 2: Serve with nginx
FROM nginx:alpine

COPY --from=builder /app/dist /usr/share/nginx/html
COPY containers/web/nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 3000

CMD ["nginx", "-g", "daemon off;"]
