FROM gcc:latest

WORKDIR /app

COPY . .

RUN g++ main.cpp -o procwatch

CMD ["./procwatch", "list"]