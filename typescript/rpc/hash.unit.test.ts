import { hashCode } from "./hash";


describe('hashcode', () => {
    it('should correctly do hash', () => {
        const expectedCode = 1439082586;
        const result = hashCode('Something');

        expect(result).toEqual(expectedCode);
    });
})