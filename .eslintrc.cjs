/* eslint-env node */
module.exports = {
  root: true,
  ignorePatterns: ['node_modules/'],
  overrides: [
    {
      files: ['packages/**/*.{ts,tsx}'],
      parser: '@typescript-eslint/parser',
      parserOptions: {
        ecmaVersion: 'latest',
        sourceType: 'module',
      },
      plugins: ['@typescript-eslint', 'import'],
      extends: [
        'eslint:recommended',
        'plugin:@typescript-eslint/recommended',
        'plugin:import/recommended',
        'plugin:import/typescript',
        'prettier',
      ],
      settings: {
        'import/resolver': {
          typescript: {
            alwaysTryTypes: true,
            project: ['packages/*/tsconfig.json'],
          },
        },
      },
      rules: {
        '@typescript-eslint/explicit-module-boundary-types': 'off',
      },
    },
    {
      files: ['packages/**/*.{js,jsx,cjs,mjs}'],
      parserOptions: {
        ecmaVersion: 'latest',
        sourceType: 'module',
      },
      extends: ['eslint:recommended', 'plugin:import/recommended', 'prettier'],
      plugins: ['import'],
      env: {
        es2022: true,
        node: true,
        browser: true,
      },
      settings: {
        'import/resolver': {
          node: {
            extensions: ['.js', '.jsx'],
          },
        },
      },
    },
  ],
};
