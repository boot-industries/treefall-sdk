export default {
  async fetch(request: Request): Promise<Response> {
    return new Response('Wordbird worker placeholder', {
      headers: { 'content-type': 'text/plain' },
    });
  },
};
